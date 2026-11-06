# Hyrule Research — Reconstrução e persistência do estado gráfico Fast3D

## Escopo e método

Esta análise estática considera:

- Shipwright: `f0599f96f8d442f26d0fa1c5cedafddacd56633f`
- libultraship: `da6daf1a50a5e46d6641408d52227e31e85e46df`

Não foram alterados arquivos de código, instaladas dependências, executados builds ou iniciado o jogo. As conclusões descrevem o código observado nesses commits; não são afirmações sobre o remake de Ocarina of Time de 2026.

**Distinção importante:** `Interpreter::Run()` reinicia alguns campos, mas não recria nem zera os objetos `RSP` e `RDP` em cada passagem. A lista reaplicada reconstrói os estados que seus comandos configuram; os demais podem continuar com valores de uma execução anterior.

## 1. Ciclo de vida por passagem

Em `Graph_ProcessGfxCommands()`, o Shipwright calcula quantas vezes interpretar a lista. `RunCommands()` passa repetidamente o mesmo ponteiro `Gfx*`, atualizando o fator temporal e o índice de interpolação. Para cada chamada, `Fast3dWindow::DrawAndRunGraphicsCommands()` verifica se o frame está pronto, inicia a moldura, executa o interpretador e finaliza a apresentação:

```text
Graph_ProcessGfxCommands(commands)
  └─ RunCommands(commands, ...)
       ├─ mInterpolationIndex = 0
       └─ para cada passagem:
            ├─ mInterpolationT = fator desta passagem
            ├─ FrameInterpolation_Interpolate(fator)
            └─ DrawAndRunGraphicsCommands(mesma lista, substituições)
                 ├─ StartFrame()
                 ├─ Interpreter::Run()
                 │    ├─ SpReset()                 (reset parcial)
                 │    ├─ limpa caches de pixel depth
                 │    ├─ configura framebuffer/viewport inicial
                 │    ├─ executa os comandos em ordem
                 │    └─ Flush()
                 ├─ GUI::EndDraw()
                 └─ Interpreter::EndFrame()        (renderização e swap)
            └─ incrementa mInterpolationIndex
```

Referências: [`RunCommands()` e `Graph_ProcessGfxCommands()`](https://github.com/HarbourMasters/Shipwright/blob/f0599f96f8d442f26d0fa1c5cedafddacd56633f/soh/soh/OTRGlobals.cpp#L1793), [`DrawAndRunGraphicsCommands()`](https://github.com/HarbourMasters/libultraship/blob/da6daf1a50a5e46d6641408d52227e31e85e46df/src/fast/Fast3dWindow.cpp#L196), [`Interpreter::Run()` e `EndFrame()`](https://github.com/HarbourMasters/libultraship/blob/da6daf1a50a5e46d6641408d52227e31e85e46df/src/fast/interpreter.cpp#L5147), [`Interpreter::SpReset()`](https://github.com/HarbourMasters/libultraship/blob/da6daf1a50a5e46d6641408d52227e31e85e46df/src/fast/interpreter.cpp#L4941).

`DrawAndRunGraphicsCommands()` retorna sem chamar `Run()` se `IsFrameReady()` for falso. O chamador ainda incrementa `mInterpolationIndex` após a chamada. Portanto, o índice corresponde às tentativas no loop de `RunCommands()`, não necessariamente ao número de frames efetivamente desenhados.

`Interpreter::EndFrame()` chama `EndFrame()` do renderer e o par `SwapBuffersBegin()`/`SwapBuffersEnd()`, com `FinishRender()` entre eles. A limpeza de framebuffer e a reinicialização de viewport/scissor ocorrem em `Run()`, não em uma restauração completa do estado RSP/RDP.

## 2. Estado RSP

`RSP` é declarado em [`include/fast/interpreter.h`](https://github.com/HarbourMasters/libultraship/blob/da6daf1a50a5e46d6641408d52227e31e85e46df/include/fast/interpreter.h#L234). `Interpreter` aloca `mRsp` uma vez no construtor (`new RSP()`); em `Run()`, `SpReset()` só redefine os campos indicados na tabela. A tabela distingue **reset parcial**, **persistência** e **reconstrução por comandos**.

| Campos RSP | O que ocorre entre passagens | Comandos/uso observados |
|---|---|---|
| `modelview_matrix_stack`, `modelview_matrix_stack_size` | O tamanho é redefinido para 1; os elementos da pilha não são limpos por `SpReset()`. Assim, o elemento da base pode manter a matriz anterior até um comando de matriz alterá-lo. | `GfxSpMatrix` carrega/multiplica e pode empilhar matrizes; vértices usam `MP_matrix`. |
| `MP_matrix`, `P_matrix` | Persistem até operações de matriz atualizá-los; `SpReset()` não os inicializa por passagem. | `GfxSpMatrix`; `GfxSpVertex` usa a matriz combinada para transformar posição. |
| `lookat[2]`, `current_lookat_coeffs[2][3]` | `SpReset()` escreve direções padrão em `lookat` e recalcula os coeficientes. | `GfxSpMovemem*` pode alterar look-at; `GfxSpVertex` usa coeficientes para iluminação e geração de textura. |
| `current_lights`, `current_lights_coeffs` | Os registros de luz persistem; `lights_changed` é marcado verdadeiro, levando ao recálculo dos coeficientes quando a iluminação por vértice os demanda. Os valores das luzes dependem de comandos de carga. | `GfxSpMovemem*`; `GfxSpVertex`. |
| `current_num_lights`, `lights_changed` | São redefinidos para 2 e `true` em `SpReset()`. | Comandos `G_MW_NUMLIGHT` atualizam quantidade; carregar luzes atualiza o estado de iluminação. |
| `geometry_mode`, `extra_geometry_mode` | Persistem; não são zerados em `SpReset()`. | `GfxSpGeometryMode` e `GfxSpExtraGeometryMode` aplicam operações de limpar/ativar bits. |
| `fog_mul`, `fog_offset` | Persistem até o comando de fog alterar os registradores. | `G_MW_FOG`; consumidos durante a preparação de vértices/estado gráfico. |
| `texture_scaling_factor` | Persiste até `GfxSpTexture()` escrever `s` e `t`. | Afeta coordenadas UV calculadas por `GfxSpVertex`. |
| `loaded_vertices[MAX_VERTICES + 4]` | Persiste como armazenamento; `G_VTX` sobrescreve os índices carregados e `G_MODIFYVTX` modifica o slot indicado. Os slots não escritos não são limpos por passagem. | `GfxSpVertex`, `GfxSpModifyVertex`, `GfxSpTri1`. |

No código, `SpReset()` define o tamanho da pilha model-view, número de luzes, flag de recálculo e vetores look-at; não limpa matrizes, vértices, modos de geometria ou registradores de fog. `GfxSpMatrix()` procura uma substituição interpolada pela identidade/endereço da matriz e, se não houver, decodifica a matriz do comando. `GfxSpVertex()` transforma e grava vértices; triângulos consultam esses slots depois. Referências: [`GfxSpMatrix()`](https://github.com/HarbourMasters/libultraship/blob/da6daf1a50a5e46d6641408d52227e31e85e46df/src/fast/interpreter.cpp#L1485), [`GfxSpVertex()`](https://github.com/HarbourMasters/libultraship/blob/da6daf1a50a5e46d6641408d52227e31e85e46df/src/fast/interpreter.cpp#L1580), [`GfxSpTri1()`](https://github.com/HarbourMasters/libultraship/blob/da6daf1a50a5e46d6641408d52227e31e85e46df/src/fast/interpreter.cpp#L1779).

## 3. Estado RDP

`RDP` é declarado em [`include/fast/interpreter.h`](https://github.com/HarbourMasters/libultraship/blob/da6daf1a50a5e46d6641408d52227e31e85e46df/include/fast/interpreter.h#L261). Não há uma rotina equivalente a `SpReset()` que percorra e zere seus campos em `Interpreter::Run()`.

| Campos RDP | O que ocorre entre passagens | Comandos/uso observados |
|---|---|---|
| `palettes[2]`, `palette_dram_addr[2]`, `palette_staging[2][256]` | Ponteiros e staging persistem. `GfxDpLoadTlut()` copia entradas na staging e atualiza os ponteiros; a área não é limpa por passagem. | `G_LOADTLUT`; importação CI e construção da chave de cache. |
| `texture_to_load` (`addr`, `siz`, `width`, `tex_flags`, `raw_tex_metadata`) | Persiste até `GfxDpSetTextureImage()` definir os dados da imagem seguinte. | `G_SETTIMG`, seguido por comandos de carga. |
| `loaded_texture[2]` | Endereço, tamanhos, stride, flags, metadados e estados `masked`/`blended` persistem. `G_LOADBLOCK`/`G_LOADTILE` atualizam o slot correspondente; o upload só é solicitado mais tarde quando o estado de textura é consumido por um triângulo. | `ImportTexture()` e os handlers de carga. |
| `texture_tile[8]` | Formato, tamanho, TMEM, paleta, wrap/clamp, shifts, dimensões e índice persistem. `G_SETTILE`, `G_SETTILESIZE`, `G_LOADTILE` e comandos especiais alteram campos específicos. | `GfxDpSetTile()`, `GfxDpSetTileSize()`, `GfxDpLoadTile()`, handlers de tile interpolado. |
| `textures_changed[2]`, `first_tile_index` | As flags podem ser marcadas pelos comandos de tile/textura/carga; quando uma textura usada é importada, a flag correspondente é limpa. `first_tile_index` persiste até `GfxSpTexture()` atualizá-lo. | Importação de textura ao processar o triângulo e seleção de tiles/samplers. |
| `other_mode_l`, `other_mode_h`, `combine_mode`, `grayscale` | Persistem até comandos de estado alterá-los. | `GfxDpSetOtherMode()`, `GfxSpSetOtherMode()`, `GfxDpSetCombineMode()` e handlers de estado. Influenciam shader, blending e profundidade. |
| Frações e cores (`prim_lod_fraction`, `prim_depth`, env/prim/fog/blend/fill/grayscale) | Persistem até o comando correspondente escrever novos valores. | Comandos `G_SET*COLOR`, `G_SETZ` e consumidores no triângulo/shader. |
| `key_center`, `key_scale`, `convert_k[6]` | Persistem; são atualizados por comandos de chroma key/conversão quando presentes. | Handlers RDP correspondentes. |
| `viewport`, `scissor`, `viewport_or_scissor_changed` | Valores geométricos persistem; `Run()` marca o par como alterado e zera as cópias de viewport/scissor em `mRenderingState`, forçando a comparação/aplicação quando um triângulo os consome. | Comandos de viewport/scissor e caminho de `GfxSpTri1()`. |
| `z_buf_address`, `color_image_address` | Persistem até comandos de imagem atualizarem os endereços. | `GfxDpSetZImage()` e `GfxDpSetColorImage()`. |

Referências: [`GfxDpSetTextureImage()` e `GfxDpSetTile()`](https://github.com/HarbourMasters/libultraship/blob/da6daf1a50a5e46d6641408d52227e31e85e46df/src/fast/interpreter.cpp#L2466), [`GfxDpLoadTlut()`](https://github.com/HarbourMasters/libultraship/blob/da6daf1a50a5e46d6641408d52227e31e85e46df/src/fast/interpreter.cpp#L2520), [`GfxDpLoadBlock()` e `GfxDpLoadTile()`](https://github.com/HarbourMasters/libultraship/blob/da6daf1a50a5e46d6641408d52227e31e85e46df/src/fast/interpreter.cpp#L2561), [`GfxDpSetOtherMode()`](https://github.com/HarbourMasters/libultraship/blob/da6daf1a50a5e46d6641408d52227e31e85e46df/src/fast/interpreter.cpp#L3101).

### Estado de apresentação fora de RSP/RDP

`RenderingState` também persiste como membro de `Interpreter`. Contém flags de depth test/write, decal e alpha, cópias de viewport/scissor, ponteiro de shader e ponteiros para entradas do cache de textura. Em `Run()`, viewport/scissor são zerados para forçar atualização; os demais campos não são globalmente zerados ali. Os setters de estado do renderer são chamados quando o estado calculado difere do valor guardado. `Flush()` envia o buffer de triângulos e zera seus contadores; o array de buffer é reutilizado. Veja [`RenderingState` e membros do interpretador](https://github.com/HarbourMasters/libultraship/blob/da6daf1a50a5e46d6641408d52227e31e85e46df/include/fast/interpreter.h#L340) e [`Flush()`/comparações em `GfxSpTri1()`](https://github.com/HarbourMasters/libultraship/blob/da6daf1a50a5e46d6641408d52227e31e85e46df/src/fast/interpreter.cpp#L137).

## 4. Dependências de ordem entre comandos

O interpretador percorre a lista sequencialmente; handlers alteram estados que os comandos seguintes consomem. Exemplos observáveis:

1. **Matriz → vértices → triângulo.** `GfxSpMatrix()` altera matrizes de transformação; `GfxSpVertex()` transforma e guarda os vértices carregados; `GfxSpTri1()` lê os slots especificados. Um triângulo não reconstrói os vértices nem recupera a matriz que faltava.
2. **Imagem → tile → carga → uso.** `GfxDpSetTextureImage()` define a origem; `GfxDpSetTile()` define formato/modo/stride; `GfxDpLoadBlock()` ou `GfxDpLoadTile()` atualizam o slot carregado; na hora de desenhar, `ImportTexture()` usa esses dados se o combinador efetivamente precisa da textura e a flag estiver marcada.
3. **TLUT → textura CI.** `GfxDpLoadTlut()` atualiza staging/paletas. A textura CI usa a paleta vigente na interpretação/importação, e a chave de cache inclui endereços de paleta.
4. **Iluminação/modo de geometria → vértices.** `GfxSpVertex()` calcula a cor quando `G_LIGHTING` está ativo, usando as luzes e os coeficientes vigentes naquele instante. Carregar ou alterar luzes depois de carregar os vértices não recalcula automaticamente os slots já preenchidos.
5. **Combine/other mode/tile → triângulo.** O triângulo deriva opções de shader e estado (como alpha/depth), escolhe texturas/samplers e monta os dados do buffer com o estado corrente. Alterar registradores após o triângulo não modifica dados já adicionados ao buffer.

## 5. Reconstrução, caches e proteções

- **Reconstrução por comandos:** o código não restaura snapshots de estado RSP/RDP antes de cada replay. As operações presentes na lista são reaplicadas na ordem. Campos sem setter na lista permanecem disponíveis com seus valores anteriores.
- **Reset parcial:** `SpReset()` redefine a pilha de shader, tamanho da pilha model-view, número de luzes e valores look-at; não limpa todo o RSP e não limpa o RDP.
- **Estado de apresentação:** `Run()` limpa o framebuffer e força viewport/scissor a serem reavaliados. No final, há tratamento de segurança que volta ao framebuffer principal se o display list deixar outro framebuffer ativo; esse fail-safe não equivale a restaurar todos os registradores.
- **Bufferização:** `GfxSpTri1()` acumula vértices em `mBufVbo`; alterações de estado que exigem ordenação chamam `Flush()`. A lista repetida torna a montar os triângulos. O armazenamento é reutilizado, mas os contadores são zerados ao enviar e no fim de `Run()`.
- **Cache de texturas:** `TextureCacheLookup()` consulta uma chave com endereço de textura/paleta, formato, tamanho e bytes. Acertos selecionam o ID existente; falhas alocam ou reutilizam um ID, com limite e política LRU. A importação é evitada no acerto, exceto que comandos de invalidação podem limpar entradas.
- **Cache de shader/combiner:** combinadores e programas ficam em caches do interpretador/backend. `LookupOrCreateShaderProgram()` procura um programa existente antes de criar/carregar um novo; não há recompilação obrigatória em cada passagem.
- **Comandos especiais:** `G_SETTILESIZE_LERP` altera coordenadas do tile em função de `mInterpolationT`; `G_SETTILESIZE_INTERP` executa a configuração associada apenas quando o índice coincide com `mInterpolationIndex`; `G_MW_SEGMENT_INTERP` também condiciona a alteração de ponteiro a um índice. São proteções/seleções locais, não uma restauração global.
- **Lista mutável:** `gfx_vtx_hash_handler_custom` resolve um recurso e grava o endereço do vértice em `cmd->words.w1`. Portanto, ao menos esse handler modifica a lista durante a interpretação; não existe garantia geral de imutabilidade do `Gfx*`.

Referências: [cache de combiner](https://github.com/HarbourMasters/libultraship/blob/da6daf1a50a5e46d6641408d52227e31e85e46df/src/fast/interpreter.cpp#L433), [importação/cache de textura](https://github.com/HarbourMasters/libultraship/blob/da6daf1a50a5e46d6641408d52227e31e85e46df/src/fast/interpreter.cpp#L1250), [handler de invalidação](https://github.com/HarbourMasters/libultraship/blob/da6daf1a50a5e46d6641408d52227e31e85e46df/src/fast/interpreter.cpp#L3333), [handler de vértice customizado](https://github.com/HarbourMasters/libultraship/blob/da6daf1a50a5e46d6641408d52227e31e85e46df/src/fast/interpreter.cpp#L3635), [handlers de interpolação de tile](https://github.com/HarbourMasters/libultraship/blob/da6daf1a50a5e46d6641408d52227e31e85e46df/src/fast/interpreter.cpp#L4310).

## 6. Estados persistentes e riscos de inconsistência no replay

### Fatos observados

- `RSP` e `RDP` são alocados no construtor do interpretador, e não dentro de `Run()`.
- `SpReset()` é parcial. Em particular, não redefine `geometry_mode`, matrizes, slots de vértice, textura, estados RDP ou a maior parte dos dados de iluminação.
- Entre os dados transitórios do frame, `Run()` limpa os caches de pixel depth identificados no código e configura framebuffer/viewport inicial; não percorre todos os membros de RSP/RDP para restaurá-los.
- Um triângulo pode consumir valores que foram escritos por comandos anteriores: vértices, modos, texturas, combine mode, cores e profundidade.
- As flags de `textures_changed` controlam importação adiada; não constituem limpeza de todos os dados de textura.
- Há comparações de estado para reduzir chamadas repetidas ao backend, mas elas não substituem comandos que deveriam estabelecer o estado lógico.
- A interpolação altera entradas por passagem: mapa de matrizes, fator `mInterpolationT` e comandos condicionados por índice. Portanto, mesmo a mesma lista não necessariamente percorre o mesmo estado em todas as passagens.

### Riscos condicionais (não resultados medidos)

- Se uma lista não configurar uma matriz antes de carregar vértices, o cálculo usará a matriz que estiver no estado do RSP. Como a pilha é persistente e somente seu tamanho é reiniciado, isso pode depender da passagem anterior.
- Se um triângulo referenciar um slot que não foi atualizado por `G_VTX` na execução corrente, o slot permanece disponível no array. A consequência dependerá do conteúdo deixado e da lista efetivamente executada.
- Se faltarem comandos de textura/modo/cor/depth necessários, o valor consumido pode ser o de uma lista anterior, pois esses campos do RDP não são limpos por `Run()`.
- A diferença de fator ou índice entre replay pode fazer handlers especiais escolherem entradas distintas; se `IsFrameReady()` descartar uma passagem, o índice ainda avança no chamador.
- Comandos de invalidação de cache ou handlers que escrevem em memória/lista são reexecutáveis; nenhuma barreira geral os torna idempotentes.

Esses são riscos derivados do fluxo e do armazenamento observados, não afirmações de que as display lists válidas do Shipwright necessariamente omitem os comandos exigidos ou apresentem defeitos.

## 7. Limitações e conclusão

Esta leitura cobre as estruturas `RSP`, `RDP` e os caminhos de execução destacados, não prova o estado interno de cada implementação concreta de backend nem enumera todas as combinações possíveis de microcódigo. A inicialização da API pode realizar operações próprias; não se presume aqui que ela restaure ou preserve algum registrador além do que o código do interpretador mostra.

**Conclusão:** a arquitetura é stateful e sequencial. O replay começa com reset parcial e configurações de frame, então a lista reconstrói os campos que efetivamente escreve. O restante continua persistente. Há mecanismos locais — flags `textures_changed`, comparações de `RenderingState`, caches, `Flush()` e handlers condicionais de interpolação — mas não foi encontrado um snapshot/restore geral ou uma validação que assegure que todo estado necessário seja reemitido antes de cada uso.

### Arquivos analisados

- [`src/fast/interpreter.cpp`](https://github.com/HarbourMasters/libultraship/blob/da6daf1a50a5e46d6641408d52227e31e85e46df/src/fast/interpreter.cpp): ciclo `StartFrame`/`Run`/`EndFrame`, `SpReset`, handlers RSP/RDP, desenho, estado e caches.
- [`include/fast/interpreter.h`](https://github.com/HarbourMasters/libultraship/blob/da6daf1a50a5e46d6641408d52227e31e85e46df/include/fast/interpreter.h): declarações integrais de `RSP`, `RDP`, `RenderingState`, cache e campos persistentes do interpretador.
- [`src/fast/Fast3dWindow.cpp`](https://github.com/HarbourMasters/libultraship/blob/da6daf1a50a5e46d6641408d52227e31e85e46df/src/fast/Fast3dWindow.cpp): frame da janela, descarte quando não está pronto, execução da lista e finalização.
- [`soh/soh/OTRGlobals.cpp`](https://github.com/HarbourMasters/Shipwright/blob/f0599f96f8d442f26d0fa1c5cedafddacd56633f/soh/soh/OTRGlobals.cpp): agenda do replay e transmissão de fator/índice de interpolação.
- [`soh/soh/frame_interpolation.cpp`](https://github.com/HarbourMasters/Shipwright/blob/f0599f96f8d442f26d0fa1c5cedafddacd56633f/soh/soh/frame_interpolation.cpp): gravação dos estados anterior/atual e produção do mapa de substituições de matrizes.
