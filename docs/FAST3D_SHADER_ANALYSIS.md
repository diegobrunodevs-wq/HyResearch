# Hyrule Research — Investigação de Shaders do Fast3D

## Escopo e método

Esta análise considera os documentos de referência do Hyrule Research, a investigação anterior do pipeline Fast3D e o submódulo `libultraship` no commit fixado pelo Shipwright 9.3.1:

- Shipwright: `f0599f96f8d442f26d0fa1c5cedafddacd56633f`
- libultraship: `da6daf1a50a5e46d6641408d52227e31e85e46df`

A análise é estática. Não foram modificados arquivos, instaladas dependências, executados builds ou iniciado o jogo.

**Distinção central:** o interpretador traduz comandos e estados gráficos associados ao N64 em características de shader; o Prism processa templates; o backend gera e compila código específico da API; a GPU executa o shader resultante. Isso não demonstra equivalência com o RSP/RDP original nem com o remake de Switch 2.

## 1. Como o estado N64 determina shaders

Em `src/fast/interpreter.cpp`, `Interpreter::GfxSpTri1` lê o Combine Mode e diversos campos RDP/RSP e os codifica em opções de shader. Entre as características consideradas estão ciclo de combinação, alpha, fog, textura de borda, dithering/noise, grayscale, profundidade primitiva e uso de texturas mascaradas ou blended.

O resultado é uma chave formada por dois identificadores. O interpretador consulta o cache da API gráfica; se não houver programa para aquela combinação, solicita sua criação. A função `gfx_cc_get_features`, no mesmo arquivo, decodifica os identificadores em `CCFeatures`: seletores do combiner, opções, texturas usadas, clamp e número de inputs.

O arquivo `include/fast/interpreter.h` declara essa estrutura e a função.

**Fato observado:** mudanças no estado N64 podem levar a outra chave e, portanto, a outro programa especializado. A chave também inclui características que afetam a forma do shader, como texturas e clamp.

### Seleção adicional de shaders

`gfx_push_shader` e `gfx_get_shader`, em `interpreter.cpp`, mantêm uma pilha de seleções. Os backends consultam essa seleção e, quando há um nome, tentam carregar o recurso correspondente em vez do template padrão.

Isso é uma seleção de shader por recurso; não significa que o interpretador traduza um shader arbitrário para todas as APIs.

## 2. Interpretação do Combine Mode

Em `Interpreter::GenerateCC`, em `src/fast/interpreter.cpp`, o Combine Mode é dividido em seletores RGB e alpha para cada ciclo. O código normaliza alguns casos e, no modo de um ciclo, remapeia certos usos de `TEXEL1` para `TEXEL0`. Em seguida, mapeia muxes como texels, primitive, shade, environment e combined para entradas internas e gera os identificadores do shader.

Os templates codificam o cálculo geral do combiner na forma:

`(A - B) * C + D`

O helper `prism_append_formula`, em `src/fast/backends/gfx_direct3d11.cpp`, constrói a expressão. Ele escolhe formas simplificadas quando os seletores permitem — apenas `D`, multiplicação ou interpolação — e, nos demais casos, gera a expressão geral. O backend OpenGL tem helper equivalente em `src/fast/backends/gfx_opengl.cpp`.

Os templates de destino são:

- `src/fast/shaders/directx/default.shader.hlsl`
- `src/fast/shaders/opengl/default.shader.glsl`

Ambos incluem caminhos condicionais para um ou dois ciclos e para os recursos usados pelo shader.

**Limite observado:** `GenerateCC` tem um caminho que registra em `stderr` um `ccmux` não suportado. Portanto, não se deve inferir suporte irrestrito a qualquer combinação possível de muxes.

## 3. Iluminação, fog, transparência e profundidade

### Iluminação

Em `src/fast/interpreter.cpp`, `GfxSpVertex` transforma vértices e, quando `G_LIGHTING` está ativo, calcula a cor do vértice a partir de normais e luzes. O código trata luzes posicionais e não posicionais. O resultado é gravado na cor `SHADE`, que pode ser usada como uma entrada do Combine Mode.

**Conclusão baseada no fluxo observado:** a iluminação descrita é calculada no interpretador por vértice, antes do shader de fragmento. Isso não é evidência de iluminação moderna per-pixel ou de um modelo PBR.

### Fog

O interpretador calcula um fator de fog por vértice e o envia junto com a cor de fog. Os templates GLSL/HLSL misturam a cor do fragmento com essa cor usando o fator como peso. Assim, o estado de fog influencia a especialização do shader e os dados enviados por vértice.

### Transparência e alpha

O interpretador deriva uma opção de alpha de determinados bits do blender RDP. Essa opção influencia tanto o cálculo de alpha no combiner quanto o comportamento do backend.

Nos templates, o alpha pode ser calculado, afetado por noise, descartado abaixo do limiar de `8/256` ou transformado em zero pelo caminho `invisible`. A opção de texture edge usa um limiar de alpha e descarta fragmentos abaixo dele.

É importante distinguir **produzir alpha no shader** de **compor com o framebuffer**:

- O OpenGL ativa/desativa `GL_BLEND` em `SetUseAlpha` e configura `GL_SRC_ALPHA`/`GL_ONE_MINUS_SRC_ALPHA` em sua inicialização, em `src/fast/backends/gfx_opengl.cpp`.
- O D3D11 cria um blend state associado ao programa, habilitado quando `CCFeatures::opt_alpha` está ativo, em `src/fast/backends/gfx_direct3d11.cpp`. `SetUseAlpha` é um no-op nesse backend porque o estado já foi configurado como parte do programa.

**Fato:** os backends implementam o controle de blending de maneiras diferentes, embora recebam as características de shader derivadas pelo interpretador. Não se deve presumir comportamento idêntico em todos os casos.

### Profundidade

`GfxSpTri1` combina `G_ZBUFFER`, `G_ZS_PRIM`, `Z_CMP` e `Z_UPD` para decidir teste e escrita de profundidade. Também encaminha o modo decal. Os backends configuram o estado de depth test e depth write.

Os templates incluem escrita de profundidade primitiva — `SV_Depth` no HLSL e `gl_FragDepth` no GLSL — quando a opção correspondente está ativa.

Isso comprova a tradução de estados relevantes para a API moderna, mas não uma reprodução exata do comportamento temporal ou de precisão do RDP.

## 4. Como o shader chega ao Direct3D 11

O caminho está em `src/fast/backends/gfx_direct3d11.cpp`:

1. `CreateAndLoadNewShader` obtém `CCFeatures` a partir dos identificadores.
2. `gfx_direct3d_common_build_shader` processa um template HLSL com os valores dessas características.
3. O recurso padrão é `shaders/directx/default.shader.hlsl`; se há um shader selecionado, o código forma o caminho correspondente com sufixo `.hlsl`. O carregamento usa o `ResourceManager`.
4. `D3DCompile` compila o resultado para `VSMain` (`vs_4_0`) e `PSMain` (`ps_4_0`).
5. O backend cria os objetos D3D11 de vertex shader, pixel shader, input layout e blend state e guarda o programa em cache.
6. Durante o desenho, o backend associa shaders, layout e recursos e envia os vértices com `Draw`.

Essas etapas distinguem **geração** de HLSL, **compilação** para bytecode D3D e **execução** ao desenhar. O D3D11 não recebe o Combine Mode N64 diretamente.

## 5. Papel do `prism-processor`

O README do submódulo lista `prism-processor` como preprocessor de shaders. Seu uso está presente nos dois backends examinados:

- No caminho D3D11, `src/fast/backends/gfx_direct3d11.cpp` popula um contexto com `CCFeatures`, disponibiliza callbacks como `append_formula` e `update_floats`, carrega o template e chama `processor.process()`.
- No caminho OpenGL, `src/fast/backends/gfx_opengl.cpp` faz processo equivalente para os shaders de vértice e fragmento, com contexto de versão e sintaxe GLSL.
- Os templates contêm diretivas condicionais e de expansão (`@if`, `@for`, `@{...}`), demonstrando que são especializados antes da compilação.

**Conclusão:** no caminho examinado, Prism processa templates e injeta valores/expressões. A compilação para a API ocorre depois, via `D3DCompile` ou OpenGL. Não há evidência de que Prism seja o compilador D3D/GL ou um tradutor entre HLSL e GLSL.

## 6. Comparação D3D11 e OpenGL

| Etapa | D3D11 | OpenGL |
|---|---|---|
| Características | `gfx_cc_get_features` produz `CCFeatures` a partir das chaves do shader. | Usa a mesma função e estrutura. |
| Template padrão | `src/fast/shaders/directx/default.shader.hlsl` | `src/fast/shaders/opengl/default.shader.glsl` |
| Especialização | Prism processa HLSL; o backend produz VS e PS a partir do template. | Prism processa fontes GLSL de vértice e fragmento. |
| Compilação/link | Compila `VSMain` e `PSMain` com `D3DCompile`, cria objetos D3D11. | Chama `glCompileShader` para vertex e fragment e depois `glLinkProgram`. |
| Blending | Estado associado ao shader, com fatores alpha descritos no blend state. | `SetUseAlpha` alterna `GL_BLEND`; a função de blending é configurada na inicialização. |
| Execução | Associa objetos D3D11 e emite desenho pelo contexto. | Associa programa/recursos e desenha com chamadas OpenGL. |

O uso comum de `CCFeatures` demonstra uma fonte compartilhada de características. Os templates e as APIs são diferentes; o código examinado não prova equivalência pixel a pixel entre os backends.

## 7. Limitações e comportamentos não implementados

- Os dois templates contêm o comentário `TODO discard if alpha is 0?`; não há descarte explícito nessa condição nos trechos examinados.
- O interpretador registra muxes não suportados em `stderr`; isso limita a conclusão de suporte a todos os Combine Modes.
- Há comentários no cálculo de iluminação/geração de coordenadas que indicam fórmulas aproximadas ou não totalmente determinadas pelo código.
- A existência de caches e templates mostra geração de variantes no runtime, mas não demonstra que todas as combinações possíveis sejam usadas ou validadas pelo jogo.
- Nenhum desses fatos permite inferir como o remake de Switch 2 gera seus shaders ou como reproduz o RDP.

## 8. Diagrama do fluxo

```text
Estado RSP/RDP e Combine Mode
              |
              v
       interpreter.cpp
   GenerateCC / CCFeatures
              |
              v
       Chave do shader
              |
              v
        Cache gráfico
          /       \
       hit         miss
        |            |
        |            v
        |       Template + Prism
        |          /       \
        |         v         v
        |       HLSL       GLSL
        |         |          |
        |         v          v
        |     D3DCompile  glCompileShader
        |         |          |
        |         v          v
        |      Objetos    glLinkProgram
        |      D3D11         |
        |         \          /
        +----------\--------/
                    |
                    v
          Backend configura estado,
          associa recursos e desenha
                    |
                    v
                   GPU
```

## 9. Implicações para o Hyrule Research

A descoberta mais útil é a separação de responsabilidades: o estado e a semântica do combiner N64 são interpretados em CPU; Prism especializa templates em HLSL/GLSL; cada backend compila, armazena em cache e executa sua própria implementação. A iluminação observada também acontece no caminho de vértices do interpretador, não como consequência automática de um shader moderno genérico.

Shipwright é uma referência concreta de **tradução seletiva de estado N64 para backends de PC**. É útil para estudar fronteiras de compatibilidade e estratégias de geração de variantes, mas não deve ser tratado como especificação do pipeline do Switch 2 ou como evidência de que seus shaders usam o mesmo modelo.
