# Hyrule Research — Fundamentos de recompilação estática ARM64 para x86-64

## Escopo e método

Este documento descreve fundamentos para investigar uma possível tradução estática de código AArch64 para código nativo x86-64 em Windows 11. Não usa executáveis, dumps, dados protegidos ou recursos do jogo; o exemplo de tradução é sintético.

**AArch64 como origem é uma hipótese de pesquisa, não uma característica confirmada do remake.** A presença de um PC x86-64 e de MSVC/CMake não demonstra a arquitetura, o formato ou as proteções do executável de origem. Antes de escolher um tradutor, seria necessário identificar publicamente e de forma autorizada o formato, a ISA e o ABI do artefato-alvo.

Também é importante distinguir:

- **Compilação cruzada:** compilar código-fonte disponível para outra ISA.
- **Recompilação/tradução estática de binário:** decodificar código máquina existente, reconstruir seu fluxo e gerar um novo binário para o host, sem executar cada instrução de origem em tempo de execução.
- **Tradução dinâmica/emulação:** traduzir blocos durante a execução ou interpretar instruções. O QEMU TCG, por exemplo, documenta tradução dinâmica; é uma referência comparativa, não uma implementação estática a ser assumida neste estudo.

Recompilação estática não recupera automaticamente o código-fonte original. Ela tenta preservar a semântica observável do programa sob um modelo de máquina e plataforma definidos.

## 1. Comparação arquitetural

| Aspecto | AArch64 / ARM64 | x86-64 / Windows x64 | Consequência para tradução |
|---|---|---|---|
| Instruções | AArch64 usa instruções de tamanho fixo de 32 bits, com operações e formas de endereçamento definidas pela ISA. | Instruções x86-64 têm codificação de tamanho variável. | O decodificador e a reconstrução de limites de instrução são específicos de cada ISA. O endereço da instrução de origem não pode ser tratado como endereço de instrução host. |
| Registradores inteiros | `x0`–`x30`, além de `SP`; a codificação `x31` representa `SP` ou o registrador zero conforme a instrução. `w0`–`w30` acessam os 32 bits baixos e zeram os bits altos ao escrever. | 16 registradores de uso geral, incluindo `RSP`, com subregistradores e efeitos de escrita dependentes do tamanho. | É preciso mapear ou guardar mais estado de origem do que cabe diretamente nos registradores host; escritas parciais e valores zero-extension exigem cuidado. |
| Registrador de retorno | `x30` é o link register (LR), usado por `BL`/`BLR` e `RET`. | Chamadas nativas normalmente empilham o endereço de retorno e `RET` o retira da pilha. | Chamadas internas traduzidas precisam manter o LR guest e o endereço de retorno host coerentes; uma função de fronteira simples pode usar o ABI x64, mas não cobre todos os fluxos guest. |
| SIMD/FP | 32 registradores `V0`–`V31`, cada um com 128 bits no SIMD/FP base; visões `B/H/S/D/Q` acessam elementos de diferentes larguras. NEON é parte do conjunto base ARMv8-A considerado nas convenções Windows ARM64. | Windows x64 usa XMM de 128 bits na base SSE2; AVX/AVX2 e outras extensões dependem do alvo e da disponibilidade de CPU/OS. | Nem a quantidade nem as regras de preservação coincidem. Instruções de lane, saturação, arredondamento, NaN e estado de controle podem exigir lowering específico ou helpers. |
| Flags/condições | `NZCV` contém Negative, Zero, Carry e oVerflow; há instruções que atualizam flags e instruções que não as atualizam. | `RFLAGS` tem flags e efeitos definidos por cada instrução x86. | Não se deve supor equivalência de flags só por usar `ADD`, `CMP` ou um salto condicional com nome parecido. Um IR pode representar flags guest explicitamente. |
| Chamada Windows | O ABI ARM64 Windows passa argumentos inteiros nos registradores `x0`–`x7` e FP/SIMD nos `v0`–`v7`, com regras adicionais para agregados e pilha. `x18` é reservado à plataforma. | Windows x64 usa posições de argumento em `RCX`, `RDX`, `R8`, `R9` e `XMM0`–`XMM3`, mais a área de shadow/home space de 32 bytes. | APIs e bibliotecas do host precisam de thunks/adaptadores; simplesmente chamar uma função com o ABI de origem é incorreto. |
| Pilha | Os ABIs Windows ARM64 e AArch64 estabelecem requisitos próprios de alinhamento, layout, registradores não voláteis e unwind. | Windows x64 tem regras próprias para alinhamento, shadow space, prólogos/epílogos e metadados de unwind. | A pilha guest não é automaticamente a pilha host. Um tradutor precisa escolher entre mapear guest stack, manter um contexto guest ou converter chamadas nas fronteiras. |
| Memória e atomics | A arquitetura oferece ordenação e operações atômicas próprias; o padrão de sincronização precisa ser preservado. | x86-64 tem seu próprio modelo de ordenação e primitivas atômicas, além de instruções como `LOCK CMPXCHG`. | Uma tradução precisa respeitar a ordem observável requerida pela origem, usando operações/fences host ou helpers. Não se pode remover uma barreira apenas porque o host costuma ordenar mais fortemente. |
| Endianness | A ISA AArch64 admite modelos de endianness definidos pela arquitetura; o ABI Windows ARM64 documenta execução little-endian. | Windows x64 opera little-endian. | Se a origem também for Windows ARM64, a representação nativa de valores little-endian é compatível nesse aspecto; isso não prova que o alvo use tal ABI. Um guest big-endian requer loads/stores com conversão. |

Fontes normativas para registradores e ABI: [AAPCS64 — Arm ABI](https://github.com/ARM-software/abi-aa/blob/main/aapcs64/aapcs64.rst), [Convenções ABI Windows ARM64 — Microsoft](https://learn.microsoft.com/en-us/cpp/build/arm64-windows-abi-conventions?view=msvc-170), [Convenção de chamada x64 — Microsoft](https://learn.microsoft.com/en-us/cpp/build/x64-calling-convention?view=msvc-170) e [Intel® 64 and IA-32 Architectures Software Developer’s Manuals](https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html).

### Registros e alocação

Uma estratégia comum é representar todos os registradores guest em uma estrutura de estado explícita (`GuestState`) e manter em registradores host apenas os valores quentes. Alternativamente, um alocador pode mapear registradores guest para host e derramar (spill) os demais para memória. Em ambos os casos, branches indiretos, chamadas, exceções e helpers devem poder materializar o estado guest esperado.

Os conjuntos de registradores preservados por uma chamada variam entre ABIs. Um tradutor que chama runtime helpers host deve salvar tudo que a semântica guest exige, não confiar nas regras de preservação do ABI de origem. `x18` no Windows ARM64, por exemplo, é reservado à plataforma; não deve ser tratado como registrador guest livre sem uma camada explícita de contexto.

### SIMD

`Vn`/`Qn` guest pode ser mantido em XMM host quando a largura e semântica correspondem, mas o conjunto de registradores é menor no x64 Windows padrão. A tradução pode precisar de spills e de dividir ou recombinar lanes. Uma instrução NEON com saturação, narrowing, permutação ou regras específicas de NaN pode não ter um equivalente único em SSE2; AVX2 pode oferecer uma implementação melhor, mas aumenta os requisitos do binário. Um projeto robusto detecta recursos ou emite um baseline compatível com o hardware declarado.

## 2. Fluxo conceitual de recompilação estática

```text
Artefato de origem autorizado + metadados do formato/ABI
             │
             ▼
Identificar segmentos executáveis, relocations, símbolos e entry points
             │
             ▼
Decodificar AArch64; separar código de dados e construir fluxo de controle
             │
             ▼
Lifting para IR com estado guest explícito
             │
             ├─ análise de registradores, flags, memória, chamadas e atomics
             ├─ referências a funções/tabelas e destinos indiretos
             └─ blocos básicos e CFG
             │
             ▼
Otimizações que preservem a semântica guest
             │
             ▼
Seleção de instruções x86-64 + alocação de registradores/spills
             │
             ▼
Thunks de ABI, runtime helpers, tabelas de endereços e metadados de unwind
             │
             ▼
Link de PE/COFF para Windows + validação diferencial e testes sintéticos
```

Esse é um modelo de trabalho, não uma receita que garante tradução completa. O levantamento de código/dados e os destinos indiretos costumam determinar se uma tradução estática abrangente é possível.

### Blocos básicos e fluxo de controle

Um bloco básico é uma sequência de instruções com uma única entrada e sem transferências de controle internas, exceto possivelmente ao final. O processo conceitual:

1. Identificar endereços iniciais confiáveis: entry points, símbolos, exports, relocations e destinos conhecidos de chamadas/branches.
2. Decodificar instruções alinhadas de AArch64 (4 bytes por instrução), seguindo branches diretos, fallthroughs e retornos.
3. Marcar instruções-alvo de branch como fronteiras de bloco; criar arestas condicionais, incondicionais, de chamada e de retorno.
4. Resolver tabelas de salto e branches indiretos por relocations, análise de dados ou informação externa confiável.
5. Repetir até estabilizar o conjunto de blocos alcançáveis e classificar regiões restantes com incerteza, sem assumir que toda palavra de 32 bits seja código.

Branches diretos podem frequentemente ser ligados estaticamente a blocos traduzidos. Um `BR Xn`/`BLR Xn` indireto pode apontar para várias funções, jump tables ou destinos calculados; sem enumerar destinos ou inserir uma resolução em runtime, o tradutor pode não saber qual bloco seguir. O endereço virtual guest também pode aparecer em dados, tabelas ou código como valor, portanto não deve ser substituído mecanicamente por endereço host.

### Exemplo sintético

Considere a função AArch64 abaixo, usando o ABI Windows ARM64:

```asm
; int64_t add2(int64_t a, int64_t b)
; X0 = a, X1 = b; resultado em X0
ADD     X0, X0, X1
RET
```

Uma função compilada diretamente para a convenção Windows x64 recebe inteiros em `RCX` e `RDX`, e retorna um escalar de 64 bits em `RAX`:

```asm
; int64_t add2_x64(int64_t a, int64_t b)
LEA     RAX, [RCX + RDX]
RET
```

Esse exemplo só é correto como tradução de uma função leaf simples quando a fronteira também adapta a convenção: `X0`/`X1` são obtidos de `RCX`/`RDX`, e o resultado guest `X0` corresponde a `RAX`. O `RET` x64 retorna usando a pilha host; não demonstra que o mecanismo de retorno AArch64 (`X30`/LR) possa ser substituído por `RET` em todos os blocos.

Outro exemplo, sob o mesmo mapeamento de parâmetros, `LDR X2, [X1, #8]` pode ser implementado como `MOV RDX, [RCX + 8]`. Essa forma direta só é adequada se o endereço guest estiver representado por endereço host compatível, o acesso tiver a mesma largura/alinhamento e as condições de falha/ordenação forem equivalentes. Um modelo de memória guest pode, em vez disso, exigir uma operação helper.

## 3. Semântica de memória e execução

### Endereços, layout e endianness

O tradutor precisa definir como endereços guest correspondem a memória host:

- **Mapeamento direto:** útil quando regiões guest podem ser alocadas com layout e permissões compatíveis; ainda exige tratar limites, relocations e endereços observados pelo programa.
- **Memória guest mediada:** loads/stores passam por helpers que validam região, largura, alinhamento, endianness e efeitos de memória mapeada por dispositivo. É mais flexível, mas acrescenta custo e pode complicar otimizações.
- **Estruturas e ponteiros:** layout de structs, bitfields, padding, ponteiros de função e handles é parte do ABI/plataforma, não apenas da ISA. Uma função traduzida que chama API Win32 precisa converter esses objetos conforme as convenções host.

Mesmo se ambos os ambientes forem little-endian, o tradutor deve verificar a configuração do executável e a semântica de loads/stores; endianness comum não resolve diferenças de alinhamento, acessos atômicos, permissões nem MMIO.

### Ordenação e operações atômicas

Instruções de acquire/release, barreiras e padrões load-exclusive/store-exclusive precisam manter a relação de sincronização e a atomicidade observáveis pelo programa. Uma sequência LL/SC pode ser convertida para um compare-exchange host ou um loop de retry, mas deve reproduzir condições de sucesso/falha, ordem de memória e efeitos em caso de interferência. A escolha de uma operação x86 mais forte pode ser correta, porém não autoriza remover automaticamente operações: o compilador precisa preservar os requisitos de visibilidade e de sincronização.

Para implementação C/C++ do runtime, as ordens `relaxed`, `acquire`, `release`, `acq_rel` e `seq_cst` devem ser mapeadas deliberadamente às primitivas suportadas pelo compilador/Windows. A documentação da [LLVM sobre atomics](https://llvm.org/docs/Atomics.html) discute semântica de ordenação em IR; a [ISA Intel SDM](https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html) e o [Arm Architecture Reference Manual (A-profile)](https://developer.arm.com/documentation/ddi0487/latest) são referências para instruções e comportamento arquitetural.

### Chamadas, retornos, exceções e instruções privilegiadas

- **Chamadas internas:** `BL` grava o endereço de retorno guest em LR. O tradutor pode materializar essa chamada como sequência host se o grafo for conhecido, ou salvar/atualizar LR em `GuestState` e usar um dispatcher. Recursão, tail calls e chamadas indiretas exigem que o estado guest esteja correto em cada transição.
- **Chamadas a bibliotecas/OS:** usar wrappers que traduzam argumentos, resultados, convenções de preservação, estruturas e códigos de erro. Windows x64 exige prólogos/epílogos e unwind metadata válidos para stack walking e tratamento de exceções.
- **Exceções:** divisão por zero, acesso inválido, instrução indefinida e exceções geradas por helpers podem ter PCs/estados diferentes em host e guest. Para preservar semântica, é necessário mapear endereço/instrução host para PC e estado guest, além de decidir como a exceção é convertida em SEH/erro do programa. Em x64, a documentação MSVC descreve a importância de prólogos e dados `pdata`/`xdata` para unwind.
- **Instruções privilegiadas/sistema:** instruções que acessam system registers, controlam cache, interrompem processadores ou dependem de nível privilegiado não podem ser emitidas simplesmente em um processo user-mode Windows. Um runtime teria de reproduzir apenas efeitos observáveis permitidos, substituí-los por serviços suportados ou declarar o caso não suportado. Não é correto assumir que cada instrução privilegiada tenha um equivalente em API.
- **Barreiras e sincronização:** uma instrução de barreira que não altera dados diretamente ainda pode ser semanticamente relevante. Ela não deve ser eliminada sem prova de que a ordem observável fica preservada para todas as threads/recursos envolvidos.

## 4. Instruções sem equivalência direta

“Uma instrução de origem = uma instrução de destino” não é requisito nem geralmente possível. Tradução ocorre por semântica:

1. **Lowering direto:** uma operação aritmética ou load/store comum pode mapear para uma instrução host ou poucas instruções.
2. **Sequência equivalente:** condição, bitfield, permutação SIMD, extensão de sinal ou atualização de flags pode exigir várias instruções host.
3. **IR/estado guest:** quando a equivalência depende de flags, PC, LR, estado FP ou memória, o tradutor guarda esses valores explicitamente.
4. **Helper de runtime:** instruções raras, operações atômicas complexas ou semântica de sistema podem chamar código auxiliar.
5. **Fallback/não suportado:** se o estado requerido não pode ser observado ou reproduzido em user mode, documentar o limite; não inventar equivalência.

Além da ISA, devem ser consideradas diferenças de tamanho de operação, extensões de sinal, comportamento de shifts fora do intervalo, overflow definido/indefinido no nível da linguagem, flags, traps FP, arredondamento, subnormais e NaNs. Uma tradução que parece numericamente próxima pode divergir em casos de borda.

## 5. Limitações técnicas e incertezas

### Descoberta de código e dados

Símbolos e relocations ajudam, mas podem estar ausentes. Literais embutidos em regiões executáveis, jump tables, ponteiros codificados, branches indiretos e limites de função ambíguos podem levar a blocos ausentes ou falsas instruções. A arquitetura AArch64 ter instruções fixas facilita encontrar fronteiras de 4 bytes, mas não identifica quais palavras são código.

### Código gerado dinamicamente ou modificado

Código criado em runtime, JITs, trampolines, recompilação de bytecode e self-modifying code não pode ser completamente conhecido por uma tradução estática fechada. Possíveis respostas incluem manter um interpretador, fornecer um JIT host separado, tratar o formato dinâmico específico ou limitar explicitamente o suporte. Isso deixa de ser uma tradução estática pura para esses trechos. O manual do QEMU TCG é útil para entender por contraste como um tradutor dinâmico lida com blocos e invalidação; não prova que a mesma estratégia seja necessária ou viável para qualquer alvo.

### Dependências de hardware e sistema

MMIO, temporizadores, interrupções, cache maintenance, system registers, instruções criptográficas opcionais, comportamento de GPU/áudio e chamadas de sistema pertencem à plataforma, não apenas à ISA. Uma aplicação ARM64 Windows e uma aplicação ARM64 em outra plataforma não são intercambiáveis por compartilharem instruções AArch64. O ABI e as APIs do sistema operacional também fazem parte do problema.

### Compatibilidade e desempenho

Preservar semântica pode exigir estado guest explícito, spills, helpers e indireções que reduzam desempenho. Otimizações agressivas (por exemplo, remover loads, fundir operações ou assumir ausência de aliasing) só são válidas quando o tradutor provou as mesmas condições que o binário de origem exige. Não há base nesta pesquisa para estimar desempenho ou afirmar que a recompilação seja completa.

### Hipótese de arquitetura não confirmada

Este estudo não analisou binário algum e não conclui que o remake use ARM64, Windows, ELF, PE/COFF, uma API específica ou código estático convencional. A etapa de identificação deve depender de metadados públicos e autorização para examinar o artefato, não de analogia com o hardware ou com projetos de referência.

## 6. Conclusões para o Hyrule Research

1. O ponto de partida é identificar a ISA **e** o ABI/plataforma; “ARM64” isoladamente não define chamadas, serviços do sistema ou formato do executável.
2. A tradução requer reconstrução de fluxo de controle e estado guest, não uma substituição textual de instruções.
3. AArch64 e x86-64 diferem em registradores, flags, retorno de chamadas, SIMD e ordenação de memória; APIs host necessitam de adaptadores.
4. Código dinâmico, endereços indiretos, instruções privilegiadas e comportamento dependente de hardware são limitações centrais para uma tradução estática completa.
5. O exemplo de soma ilustra somente uma função leaf com convenção conhecida. Ele não comprova viabilidade do jogo nem compatibilidade funcional ou desempenho.

## Referências técnicas públicas

- Arm, [Procedure Call Standard for the Arm 64-bit Architecture (AAPCS64)](https://github.com/ARM-software/abi-aa/blob/main/aapcs64/aapcs64.rst) — registradores, chamadas e regras de dados AArch64.
- Microsoft, [Overview of ARM64 ABI conventions](https://learn.microsoft.com/en-us/cpp/build/arm64-windows-abi-conventions?view=msvc-170) — ABI e regras específicas de Windows ARM64, inclusive endianness e registradores.
- Microsoft, [x64 calling convention](https://learn.microsoft.com/en-us/cpp/build/x64-calling-convention?view=msvc-170) — argumentos, registradores, shadow space, alinhamento e unwind em Windows x64.
- Intel, [Intel® 64 and IA-32 Architectures Software Developer’s Manuals](https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html) — ISA, instruções, estado e modelo de programação x86.
- Arm, [Arm Architecture Reference Manual for A-profile architecture](https://developer.arm.com/documentation/ddi0487/latest) — comportamento arquitetural A-profile/AArch64.
- LLVM, [Atomic Instructions and Concurrency Guide](https://llvm.org/docs/Atomics.html) — modelo de ordenação para atomics em IR.
- QEMU, [Translator Internals / TCG](https://www.qemu.org/docs/master/devel/tcg.html) — exemplo documentado de tradução **dinâmica**, usado aqui apenas para contraste conceitual.
