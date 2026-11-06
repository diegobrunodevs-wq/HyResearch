# Experiment 004 — Native Execution Plan

**Projeto:** HyResearch  
**Experimento:** 004 — ARM64 to x86-64 Translator  
**Fase:** Planejamento da execução nativa  
**Status:** Planejado  
**Arquitetura de origem:** ARM64  
**Arquitetura de destino:** x86-64  
**Ambiente inicial:** Windows 11 x64

---

## 1. Objetivo

Evoluir o tradutor experimental de instruções ARM64 para que ele deixe de produzir apenas representações textuais de Assembly x86-64 e passe a gerar código de máquina executável.

A execução nativa permitirá comparar o resultado produzido pelo código x86-64 real com o resultado de um modelo de referência ARM64.

O objetivo desta fase não é implementar um recompilador completo, mas estabelecer uma infraestrutura mínima, controlada e verificável de geração e execução de código nativo.

## 2. Estado atual

O experimento possui:

- Representação de instruções por meio de `Instruction`.
- Parser para um subconjunto de instruções.
- Tradutor textual ARM64 para x86-64.
- Testes automatizados do parser.
- Testes automatizados do tradutor.
- Modelo de referência semântico.
- Interpretador das instruções x86-64 textuais geradas.
- Comparação dos estados finais dos registradores.
- Testes com valores extremos de 64 bits.
- 1.000 operações pseudoaleatórias reproduzíveis.

### Instruções atualmente contempladas

| Instrução | Estado |
|---|---|
| MOV | Implementada no protótipo |
| ADD | Implementada no protótipo |
| SUB | Implementada no protótipo |
| MUL | Implementada no protótipo |
| RET | Tradução textual experimental, fora da validação semântica |

A validação atual demonstra equivalência dentro do modelo simplificado implementado. Ainda não demonstra equivalência com processadores reais.

## 3. Convenção de execução

### 3.1 Interface do código gerado

A primeira implementação utilizará uma interface controlada:

```cpp
using NativeFunction = void(*)(std::uint64_t* registers);
```

A função gerada receberá um ponteiro para um vetor contendo oito registradores de 64 bits:

| Índice | Registrador ARM64 |
|---:|---|
| 0 | X0 |
| 1 | X1 |
| 2 | X2 |
| 3 | X3 |
| 4 | X4 |
| 5 | X5 |
| 6 | X6 |
| 7 | X7 |

A interface utilizará a convenção de chamada Windows x64.

O ponteiro será recebido em RCX, conforme a convenção de chamada do Windows x64.

O código gerado deverá acessar os valores por meio desse vetor, evitando depender de uma associação permanente entre registradores ARM64 e registradores físicos x86-64.

### 3.2 Registradores temporários

A primeira implementação utilizará exclusivamente registradores temporários voláteis do Windows x64, sempre que possível.

A seleção dos registradores deverá considerar:

- RAX
- RCX
- RDX
- R8
- R9
- R10
- R11

Registradores não voláteis deverão ser preservados caso sejam utilizados.

A função também deverá respeitar o alinhamento da pilha e as demais exigências da ABI Windows x64.

### 3.3 Limites

A primeira versão não oferecerá:

- Chamadas de função ARM64.
- Manipulação de pilha ARM64.
- Acesso arbitrário à memória.
- Instruções condicionais.
- Flags NZCV.
- Registradores especiais.
- Tratamento de exceções ARM64.
- Tradução de código completo de aplicações.

## 4. Arquitetura proposta

O fluxo planejado será:

```text
Instrução ARM64
       |
       v
     Parser
       |
       v
 Representação interna
       |
       v
    Translator
       |
       v
 Gerador de código x86-64
       |
       v
 Sequência de bytes
       |
       v
 Gerenciador de memória executável
       |
       v
 Execução nativa
       |
       v
 Comparação com modelo ARM64
```

Os componentes deverão ter responsabilidades separadas.

### Parser

Interpreta as instruções de origem e produz estruturas internas.

### Translator

Valida a instrução e define sua representação no destino.

### NativeCodeGenerator

Converte as operações suportadas em instruções de máquina x86-64.

### ExecutableMemory

Gerencia a alocação, proteção, execução e liberação da memória utilizada pelo código gerado.

### SemanticValidator

Compara o resultado da execução nativa com o modelo de referência.

## 5. Geração de código de máquina

A primeira versão deverá gerar código diretamente para um subconjunto restrito de operações x86-64.

O gerador deverá:

1. Receber uma instrução validada.
2. Determinar a sequência de operações x86-64 necessária.
3. Produzir os bytes correspondentes.
4. Validar o tamanho e a estrutura da sequência.
5. Disponibilizar o resultado ao gerenciador de memória executável.

A geração inicial deverá contemplar:

- MOV entre registradores e memória.
- ADD.
- SUB.
- IMUL.

As operações serão limitadas a valores de 64 bits.

Não haverá geração de código arbitrário recebido externamente.

## 6. Gerenciamento de memória executável

No Windows, a execução de código gerado exigirá gerenciamento explícito de memória.

A implementação deverá considerar:

- `VirtualAlloc` para reservar e alocar memória.
- `VirtualProtect` para alterar permissões quando necessário.
- `VirtualFree` para liberar a memória.
- Proteção contra execução de memória não autorizada.

A estratégia deverá seguir o princípio W^X:

- Memória gravável durante a geração.
- Memória executável após a finalização.
- Evitar que a mesma região permaneça gravável e executável simultaneamente.

O código deverá ser validado antes de receber permissão de execução.

A memória deverá ser liberada mesmo quando ocorrerem erros.

## 7. Segurança e isolamento

A execução nativa envolve riscos maiores que a interpretação textual.

Por isso, a implementação deverá:

- Executar somente bytes produzidos pelo próprio gerador.
- Não aceitar código arbitrário do usuário.
- Limitar o tamanho da sequência gerada.
- Validar os registradores e operandos.
- Evitar acessos fora do vetor de registradores.
- Controlar a vida útil da memória executável.
- Evitar chamadas de sistema ou funções externas no código gerado.
- Interromper a execução quando houver falha de validação.

A execução inicial será restrita ao próprio processo de teste.

## 8. Critérios de aprovação

A fase será considerada aprovada quando:

1. O gerador produzir sequências de bytes válidas para as instruções suportadas.
2. O código gerado executar corretamente no ambiente Windows x64.
3. Os resultados coincidirem com o modelo de referência.
4. Os registradores não destinados à alteração permanecerem preservados.
5. Os casos extremos de 64 bits forem aprovados.
6. Os testes pseudoaleatórios forem reproduzíveis e aprovados.
7. Erros de geração ou validação forem detectados antes da execução.
8. A memória executável for liberada corretamente.

## 9. Estratégia de testes

A validação será dividida em etapas:

### Etapa A — Codificação

Testar os bytes produzidos para cada instrução suportada.

### Etapa B — Execução isolada

Executar cada sequência nativa com entradas conhecidas.

### Etapa C — Equivalência semântica

Comparar os resultados com o modelo de referência.

### Etapa D — Robustez

Reutilizar os casos extremos e as operações pseudoaleatórias já implementadas.

### Etapa E — Integridade

Verificar preservação de registradores, limites de memória e tratamento de erros.

## 10. Limitações conhecidas

Mesmo após a aprovação desta fase, o experimento não representará um recompilador ARM64 completo.

Ainda serão necessários estudos e implementações para:

- Decodificação de instruções ARM64 reais.
- Tratamento de diferentes larguras de registradores.
- Semântica de flags.
- Endereçamento e memória.
- Instruções vetoriais.
- Controle de fluxo.
- Convenções de chamada.
- Relocação de código.
- Interação com bibliotecas e sistema operacional.
- Compatibilidade de APIs e subsistemas.

A execução nativa de algumas instruções não comprova a viabilidade de recompilar um jogo inteiro.

## 11. Próximas entregas

| Ordem | Entrega | Estado |
|---:|---|---|
| 1 | Definir interface de execução | Planejado |
| 2 | Implementar gerador x86-64 | Planejado |
| 3 | Implementar memória executável | Planejado |
| 4 | Executar primeira função gerada | Planejado |
| 5 | Integrar com modelo de referência | Planejado |
| 6 | Executar testes de robustez | Planejado |

## 12. Conclusão

Esta fase representa a transição entre um tradutor textual experimental e um protótipo capaz de gerar e executar código nativo x86-64.

O desenvolvimento será incremental, com cada componente validado separadamente.

O princípio central permanece: nenhuma tradução será considerada correta apenas porque produziu Assembly ou bytes aparentemente válidos. A aprovação dependerá de resultados verificáveis em execução e comparação com o modelo de referência.

---

**HyResearch — Experiment 004**  
*Pesquisa experimental sobre recompilação estática ARM64 para x86-64.*