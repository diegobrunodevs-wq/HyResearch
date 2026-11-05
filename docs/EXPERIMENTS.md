# Experimentos

## 003 — Análise de Assembly

A demonstração usa `calculate.hpp` e `calculate.cpp` para manter a função
matemática separada da saída feita por `iostream` em `main.cpp`. No MSVC, o
CMake habilita `/FA` somente para `calculate.cpp`, gerando um listing de
Assembly dessa unidade de compilação no diretório intermediário do build.
Configure o CMake para a arquitetura desejada (por exemplo, x64 ou ARM64) para
gerar o listing correspondente.

A chamada de demonstração `calculate(10, 20)` resulta em `70`.
