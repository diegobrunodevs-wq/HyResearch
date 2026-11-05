## Escopo e evidência

Usei `docs/REFERENCE_PROJECTS_ANALYSIS.md` como referência. O ZIP de Shipwright contém `libultraship` como submódulo vazio; para inspecionar seu conteúdo, usei o commit registrado pelo Shipwright 9.3.1: [`da6daf1a`](https://github.com/kenix3/libultraship/tree/da6daf1a50a5e46d6641408d52227e31e85e46df). O código do jogo analisado corresponde à tag 9.3.1, commit [`f0599f96`](https://github.com/HarbourMasters/Shipwright/tree/f0599f96f8d442f26d0fa1c5cedafddacd56633f).

A análise é estática: não modifiquei arquivos, instalei dependências nem executei builds. Distingo abaixo **fatos observados no código** de **sínteses ou implicações**.

## 1. Responsabilidade arquitetural

**Fato observado:** `libultraship` é uma biblioteca estática que agrupa três áreas: `libultra`, serviços de aplicação (`ship`) e processamento gráfico Fast3D (`fast`).

- [`src/CMakeLists.txt`](https://github.com/kenix3/libultraship/blob/da6daf1a50a5e46d6641408d52227e31e85e46df/src/CMakeLists.txt) constrói o alvo estático e agrega esses módulos.
- [`src/libultraship/CMakeLists.txt`](https://github.com/kenix3/libultraship/blob/da6daf1a50a5e46d6641408d52227e31e85e46df/src/libultraship/CMakeLists.txt) inclui as implementações de compatibilidade, bridges, controles, logging, janela e GUI.
- [`README.md`](https://github.com/kenix3/libultraship/blob/da6daf1a50a5e46d6641408d52227e31e85e46df/README.md) descreve a biblioteca como uma reimplementação de funções do SDK libultra para hardware moderno e documenta o carregamento de recursos a partir de arquivos.

**Síntese:** a biblioteca conecta o código do jogo a serviços do sistema hospedeiro e a um renderer moderno. Isso não demonstra que ela emule o Nintendo 64 por completo.

## 2. Abstração de funcionalidades Nintendo 64/libultra

**Fato observado:** os headers disponibilizam tipos, macros e APIs familiares ao código N64. Algumas implementações encaminham essas chamadas para serviços modernos; outras são simplificadas ou vazias.

- [`include/libultraship/libultra.h`](https://github.com/kenix3/libultraship/blob/da6daf1a50a5e46d6641408d52227e31e85e46df/include/libultraship/libultra.h) agrega headers como `os.h`, `controller.h`, `gbi.h`, `rdp.h` e `vi.h`.
- [`include/libultraship/libultra/os.h`](https://github.com/kenix3/libultraship/blob/da6daf1a50a5e46d6641408d52227e31e85e46df/include/libultraship/libultra/os.h) declara APIs `os*`, estruturas e constantes usadas pelo jogo.
- [`src/libultraship/libultra/os.cpp`](https://github.com/kenix3/libultraship/blob/da6daf1a50a5e46d6641408d52227e31e85e46df/src/libultraship/libultra/os.cpp) encaminha consultas de controle ao `ControlDeck` e calcula `osGetTime` usando o relógio do sistema, convertido para a frequência N64 indicada no código.
- [`src/libultraship/libultra/os_mesg.cpp`](https://github.com/kenix3/libultraship/blob/da6daf1a50a5e46d6641408d52227e31e85e46df/src/libultraship/libultra/os_mesg.cpp) implementa filas de mensagens em memória.
- [`src/libultraship/libultra/os_pi.cpp`](https://github.com/kenix3/libultraship/blob/da6daf1a50a5e46d6641408d52227e31e85e46df/src/libultraship/libultra/os_pi.cpp) faz uma cópia de memória para a chamada DMA PI e limita o tamanho de leitura em um caso associado a áudio.
- [`src/libultraship/libultra/os_eeprom.cpp`](https://github.com/kenix3/libultraship/blob/da6daf1a50a5e46d6641408d52227e31e85e46df/src/libultraship/libultra/os_eeprom.cpp) armazena dados EEPROM em `default.sav`.
- [`src/libultraship/libultra/os_vi.cpp`](https://github.com/kenix3/libultraship/blob/da6daf1a50a5e46d6641408d52227e31e85e46df/src/libultraship/libultra/os_vi.cpp) usa um timer SDL para notificação VI; operações como troca de buffer e definição de modo aparecem sem implementação efetiva.
- [`soh/include/z64.h`](https://github.com/HarbourMasters/Shipwright/blob/f0599f96f8d442f26d0fa1c5cedafddacd56633f/soh/include/z64.h) inclui `libultraship/libultra.h`, fazendo os tipos e definições disponíveis ao jogo.

**Limite comprovado:** há funções com `TODO`, retornos fixos ou corpos vazios nos arquivos citados. Portanto, a evidência sustenta uma **camada seletiva de compatibilidade**, não equivalência integral ao SDK ou ao hardware N64.

## 3. Comunicação entre jogo e biblioteca

**Fato observado:** há chamadas diretas a APIs de compatibilidade e interfaces de bridge para serviços de plataforma.

- [`soh/src/code/padsetup.c`](https://github.com/HarbourMasters/Shipwright/blob/f0599f96f8d442f26d0fa1c5cedafddacd56633f/soh/src/code/padsetup.c) chama `osContInit`; [`os.cpp`](https://github.com/kenix3/libultraship/blob/da6daf1a50a5e46d6641408d52227e31e85e46df/src/libultraship/libultra/os.cpp) liga essa API ao `ControlDeck`.
- [`soh/src/code/graph.c`](https://github.com/HarbourMasters/Shipwright/blob/f0599f96f8d442f26d0fa1c5cedafddacd56633f/soh/src/code/graph.c) passa o buffer `Gfx*` a `Graph_ProcessGfxCommands`.
- [`soh/soh/OTRGlobals.h`](https://github.com/HarbourMasters/Shipwright/blob/f0599f96f8d442f26d0fa1c5cedafddacd56633f/soh/soh/OTRGlobals.h) guarda um ponteiro `Ship::Context*`.
- [`soh/soh/OTRGlobals.cpp`](https://github.com/HarbourMasters/Shipwright/blob/f0599f96f8d442f26d0fa1c5cedafddacd56633f/soh/soh/OTRGlobals.cpp) cria o contexto, inicializa serviços e define `Graph_ProcessGfxCommands` com `extern "C"` para conectar o caminho C do jogo ao código C++.
- [`include/libultraship/bridge/resourcebridge.h`](https://github.com/kenix3/libultraship/blob/da6daf1a50a5e46d6641408d52227e31e85e46df/include/libultraship/bridge/resourcebridge.h) declara funções de acesso a recursos; [`src/libultraship/bridge/resourcebridge.cpp`](https://github.com/kenix3/libultraship/blob/da6daf1a50a5e46d6641408d52227e31e85e46df/src/libultraship/bridge/resourcebridge.cpp) encaminha essas chamadas ao `ResourceManager`.
- [`soh/src/code/audio_load.c`](https://github.com/HarbourMasters/Shipwright/blob/f0599f96f8d442f26d0fa1c5cedafddacd56633f/soh/src/code/audio_load.c) inclui o bridge de recursos e os headers libultra.

**Síntese:** APIs com nomes e convenções legadas mantêm compatibilidade com trechos do jogo; código C++ do port também consulta diretamente os serviços por `Context`.

## 4. Organização dos serviços de plataforma

**Fato observado:** `Ship::Context` concentra a propriedade e o acesso a serviços como configuração, recursos, controles, janela, áudio, logging e eventos.

- [`include/ship/Context.h`](https://github.com/kenix3/libultraship/blob/da6daf1a50a5e46d6641408d52227e31e85e46df/include/ship/Context.h) declara a instância e os getters dos serviços.
- [`src/ship/Context.cpp`](https://github.com/kenix3/libultraship/blob/da6daf1a50a5e46d6641408d52227e31e85e46df/src/ship/Context.cpp) implementa a inicialização desses serviços.
- [`soh/soh/OTRGlobals.cpp`](https://github.com/HarbourMasters/Shipwright/blob/f0599f96f8d442f26d0fa1c5cedafddacd56633f/soh/soh/OTRGlobals.cpp) mostra a ordem usada pelo jogo: cria `Context`, inicializa configuração e controles, monta `soh.o2r` e instala uma `Fast3dWindow`.
- [`src/ship/controller/controldeck/ControlDeck.cpp`](https://github.com/kenix3/libultraship/blob/da6daf1a50a5e46d6641408d52227e31e85e46df/src/ship/controller/controldeck/ControlDeck.cpp) implementa a camada de controle usada por chamadas `osCont*`.
- [`include/ship/audio/Audio.h`](https://github.com/kenix3/libultraship/blob/da6daf1a50a5e46d6641408d52227e31e85e46df/include/ship/audio/Audio.h) define o gerenciador e os backends de áudio; [`src/ship/audio/Audio.cpp`](https://github.com/kenix3/libultraship/blob/da6daf1a50a5e46d6641408d52227e31e85e46df/src/ship/audio/Audio.cpp) seleciona e inicializa o player.
- [`src/ship/resource/ResourceManager.cpp`](https://github.com/kenix3/libultraship/blob/da6daf1a50a5e46d6641408d52227e31e85e46df/src/ship/resource/ResourceManager.cpp) gerencia recursos; [`src/ship/resource/archive/ArchiveManager.cpp`](https://github.com/kenix3/libultraship/blob/da6daf1a50a5e46d6641408d52227e31e85e46df/src/ship/resource/archive/ArchiveManager.cpp) gerencia arquivos; [`src/ship/resource/archive/O2rArchive.cpp`](https://github.com/kenix3/libultraship/blob/da6daf1a50a5e46d6641408d52227e31e85e46df/src/ship/resource/archive/O2rArchive.cpp) trata arquivos `.o2r`.

O README descreve `.o2r` como compatível com ZIP e `.otr` como compatível com MPQ. Isso comprova o suporte documentado pela biblioteca; não implica que todos os recursos ou mods usem ambos os formatos no fluxo de execução do Shipwright.

## 5. Integração gráfica

**Fato observado:** o jogo entrega comandos `Gfx`; a ponte do Shipwright os leva ao `Fast3dWindow`, que os processa por um interpretador Fast3D e um backend gráfico selecionado.

- [`soh/src/code/graph.c`](https://github.com/HarbourMasters/Shipwright/blob/f0599f96f8d442f26d0fa1c5cedafddacd56633f/soh/src/code/graph.c) envia buffers de comandos gráficos.
- [`soh/soh/OTRGlobals.cpp`](https://github.com/HarbourMasters/Shipwright/blob/f0599f96f8d442f26d0fa1c5cedafddacd56633f/soh/soh/OTRGlobals.cpp) implementa `Graph_ProcessGfxCommands` e chama `DrawAndRunGraphicsCommands`.
- [`src/fast/Fast3dWindow.cpp`](https://github.com/kenix3/libultraship/blob/da6daf1a50a5e46d6641408d52227e31e85e46df/src/fast/Fast3dWindow.cpp) seleciona a API de janela/renderização e entrega comandos ao interpretador.
- [`include/fast/interpreter.h`](https://github.com/kenix3/libultraship/blob/da6daf1a50a5e46d6641408d52227e31e85e46df/include/fast/interpreter.h) e [`src/fast/interpreter.cpp`](https://github.com/kenix3/libultraship/blob/da6daf1a50a5e46d6641408d52227e31e85e46df/src/fast/interpreter.cpp) implementam o interpretador; [`include/fast/ucodehandlers.h`](https://github.com/kenix3/libultraship/blob/da6daf1a50a5e46d6641408d52227e31e85e46df/include/fast/ucodehandlers.h) enumera microcódigos.
- [`src/fast/backends/`](https://github.com/kenix3/libultraship/tree/da6daf1a50a5e46d6641408d52227e31e85e46df/src/fast/backends) contém implementações para Direct3D 11, DXGI, Metal, OpenGL e SDL.

**Síntese:** o código transforma/encaminha comandos gráficos do jogo a APIs nativas. Os arquivos examinados não justificam dizer que isso constitui emulação completa do RCP.

## 6. Módulos Windows e multiplataforma

### Específicos do Windows

- [`src/ship/audio/WasapiAudioPlayer.cpp`](https://github.com/kenix3/libultraship/blob/da6daf1a50a5e46d6641408d52227e31e85e46df/src/ship/audio/WasapiAudioPlayer.cpp) e [`include/ship/audio/WasapiAudioPlayer.h`](https://github.com/kenix3/libultraship/blob/da6daf1a50a5e46d6641408d52227e31e85e46df/include/ship/audio/WasapiAudioPlayer.h): saída WASAPI. [`src/ship/CMakeLists.txt`](https://github.com/kenix3/libultraship/blob/da6daf1a50a5e46d6641408d52227e31e85e46df/src/ship/CMakeLists.txt) inclui esse player somente no Windows.
- [`src/fast/backends/gfx_direct3d11.cpp`](https://github.com/kenix3/libultraship/blob/da6daf1a50a5e46d6641408d52227e31e85e46df/src/fast/backends/gfx_direct3d11.cpp) e [`src/fast/backends/gfx_dxgi.cpp`](https://github.com/kenix3/libultraship/blob/da6daf1a50a5e46d6641408d52227e31e85e46df/src/fast/backends/gfx_dxgi.cpp): renderização Direct3D 11 e janela DXGI; [`Fast3dWindow.cpp`](https://github.com/kenix3/libultraship/blob/da6daf1a50a5e46d6641408d52227e31e85e46df/src/fast/Fast3dWindow.cpp) os registra sob `_WIN32`.
- [`cmake/dependencies/windows.cmake`](https://github.com/kenix3/libultraship/blob/da6daf1a50a5e46d6641408d52227e31e85e46df/cmake/dependencies/windows.cmake): configuração de dependências do Windows, incluindo os backends Win32/DX11 do ImGui.
- [`src/ship/Context.cpp`](https://github.com/kenix3/libultraship/blob/da6daf1a50a5e46d6641408d52227e31e85e46df/src/ship/Context.cpp) é compartilhado, embora tenha blocos condicionais Windows para APIs e caminhos do sistema.

### Compartilhados ou multiplataforma

- [`include/libultraship/libultra/`](https://github.com/kenix3/libultraship/tree/da6daf1a50a5e46d6641408d52227e31e85e46df/include/libultraship/libultra) e [`src/libultraship/libultra/`](https://github.com/kenix3/libultraship/tree/da6daf1a50a5e46d6641408d52227e31e85e46df/src/libultraship/libultra): interfaces e implementações comuns de compatibilidade.
- `Context`, recursos, configuração, filas, controles SDL e o gerenciador de áudio são áreas comuns; os backends concretos variam.
- [`src/ship/audio/SDLAudioPlayer.cpp`](https://github.com/kenix3/libultraship/blob/da6daf1a50a5e46d6641408d52227e31e85e46df/src/ship/audio/SDLAudioPlayer.cpp) é o player SDL compartilhado. [`Audio.cpp`](https://github.com/kenix3/libultraship/blob/da6daf1a50a5e46d6641408d52227e31e85e46df/src/ship/audio/Audio.cpp) seleciona WASAPI no Windows, CoreAudio no Apple, SDL ou player nulo conforme a plataforma e a configuração.
- OpenGL e SDL aparecem como caminhos gráficos compartilhados; `Fast3dWindow.cpp` também registra Metal em sistemas Apple com suporte.
- [`src/ship/audio/CoreAudioAudioPlayer.cpp`](https://github.com/kenix3/libultraship/blob/da6daf1a50a5e46d6641408d52227e31e85e46df/src/ship/audio/CoreAudioAudioPlayer.cpp) e [`src/fast/backends/gfx_metal.cpp`](https://github.com/kenix3/libultraship/blob/da6daf1a50a5e46d6641408d52227e31e85e46df/src/fast/backends/gfx_metal.cpp) são específicos de Apple. A CMake inclui também caminhos para utilitários Apple e uma implementação móvel para Android/iOS.

## Diagrama simplificado

-
-
-
-

*

## Principais descobertas para o Hyrule Research

**Fatos:** Shipwright separa lógica do jogo, compatibilidade libultra, serviços de aplicação e processamento gráfico. A compatibilidade observada é seletiva, com funções legadas adaptadas a recursos do hospedeiro e algumas operações vazias ou incompletas.

**Implicação de pesquisa:** libultraship é uma referência concreta para estudar portabilidade nativa e fronteiras entre jogo, SDK legado, serviços e renderer. Como já aponta o documento de referência, ela é voltada ao ambiente N64/OOT e **não deve ser tratada como uma camada pronta de compatibilidade para Switch 2**.