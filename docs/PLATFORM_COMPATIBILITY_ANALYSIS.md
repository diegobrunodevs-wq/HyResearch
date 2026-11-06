# Hyrule Research — Camada de compatibilidade de sistema

## Escopo e premissas

Este documento propõe uma arquitetura conceitual para adaptar código recompilado de uma plataforma ainda não identificada ao ambiente de destino informado: Windows 11 x64, MSVC, C++20, CMake, AMD Ryzen 5 5600 e NVIDIA RTX 4060.

**A arquitetura ARM64 da origem é uma hipótese; não está confirmada.** Tampouco se presume que o programa de origem use APIs, convenções ou serviços específicos de um console. Uma camada de compatibilidade só pode ser especificada com precisão depois de documentados ISA, ABI, formato de executável, APIs chamadas, modelo de memória e contratos temporais do alvo.

O documento descreve alternativas técnicas públicas, não implementa módulos. Ele diferencia:

- **API nativa de sistema:** por exemplo Win32/Windows Core Audio.
- **Biblioteca multiplataforma:** por exemplo SDL2, que pode esconder diferenças de backends.
- **API de GPU:** por exemplo Direct3D ou Vulkan, que oferece operações gráficas e sincronização GPU, mas não substitui memória, input, relógios ou threads do sistema.
- **Interface proposta:** contrato hipotético do HyResearch, ainda não existente nem validado no projeto.

## 1. Arquitetura conceitual proposta

```text
            Código recompilado + estado/ABI de origem
                         │
              CompatibilityRuntime
     ┌───────────┬─────────────┬───────────┬──────────┐
     │           │             │           │          │
 PlatformMemory PlatformThreading PlatformClock PlatformInput
     │           │             │           │
     └───────────┴─────────────┴───────────┴──────────┘
                         │
              PlatformAudio ─── áudio do SO/dispositivo
                         │
            PlatformGraphics ── API de GPU ── GPU/presentação
                         │
              Adaptadores Windows 11 x64
       (Win32 / Core Audio / DXGI / D3D / ou SDL)
```

Separação sugerida:

1. **CompatibilityRuntime** mantém o contexto de execução de origem — quando necessário — e coordena as fronteiras ABI, callbacks, estado de exceção e logging.
2. **PlatformMemory, Threading, Clock e Input** expõem contratos pequenos e independentes das APIs concretas. Os adaptadores Windows implementam esses contratos.
3. **PlatformAudio** e **PlatformGraphics** têm ciclos de vida e sincronização próprios. Não devem ser chamados como se fossem subrotinas síncronas da lógica de gameplay sem que o contrato de origem o permita.
4. Um **AudioDevice** ou uma **GraphicsDevice** não deve expor handles nativos para o código recompilado, exceto em uma fronteira explícita de integração.

## 2. Gerenciamento de memória

### Conceitos

- **Memória virtual:** processo trabalha com endereços virtuais; o sistema operacional e a MMU traduzem-nos para páginas físicas. Um endereço virtual não é um endereço físico.
- **Reserva e commit:** em Windows, `VirtualAlloc` pode reservar espaço no espaço virtual ou tornar páginas comprometidas; o sistema escolhe e administra a memória física e o paging. Aplicações ordinárias não devem presumir controle de endereços físicos.
- **Permissões:** páginas podem ter proteção de leitura, escrita e execução. Mapear código, dados e regiões de I/O exige respeitar permissões e políticas do processo.
- **Alocadores:** `new`/`delete`, `malloc`/`free` e arenas C++ operam sobre memória de processo; APIs de memória virtual são relevantes quando é necessário controle explícito de páginas, regiões reservadas ou permissões.
- **Alinhamento:** tipo, instrução SIMD, estruturas ABI e APIs gráficas podem impor requisitos diferentes. Um buffer corretamente alinhado para `alignof(T)` não é automaticamente alinhado para qualquer requisito de GPU.
- **Memória compartilhada:** pode ser implementada com objetos de mapeamento e permissões do SO. Compartilhar páginas não cria, por si só, sincronização de acesso nem coerência com uma GPU.

Microsoft documenta que cada processo tem espaço virtual privado, páginas/proteções e que `VirtualAlloc` reserva/commita regiões; a memória física é abstraída pelo sistema. Veja [Virtual Address Space](https://learn.microsoft.com/en-us/windows/win32/memory/virtual-address-space), [Memory Protection](https://learn.microsoft.com/en-us/windows/win32/memory/memory-protection) e [`VirtualAlloc`](https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-virtualalloc).

### O que pode exigir adaptação

| Comportamento de origem a identificar | Possível contrato `PlatformMemory` |
|---|---|
| Alocar/liberar blocos com tamanho e alinhamento | `allocate(size, alignment)`, `deallocate(ptr, size, alignment)`; documentar propriedade, zero-init e falhas. |
| Reservar regiões virtuais e depois habilitar páginas | Reserva/commit/protection separados; a implementação Windows pode usar `VirtualAlloc`/`VirtualProtect`, sem prometer endereço físico. |
| Páginas executáveis ou código modificado | Política separada de permissões e invalidação; compatibilidade com DEP/mitigations. Não conceder execução como fallback genérico. |
| Endereços fixos/aliasing de memória guest | `GuestAddressSpace` com base, regiões e tradução explícitas. Não converter ponteiros truncando-os para 32 bits. |
| Heap compartilhado entre threads/processos | Região compartilhada + sincronização e ciclo de vida próprios. |
| Buffers acessados pela GPU | Interface do módulo gráfico para criar recurso e transferir dados; não tratar endereço CPU como endereço GPU. |

**Limite:** `PlatformMemory` pode representar heap e espaços virtuais da aplicação, mas não deve afirmar equivalência de memória física, cache de hardware, MMIO ou memória de dispositivo sem evidência do contrato de origem.

## 3. Threads e sincronização

### C++20 versus primitivas Windows

| Necessidade | C++20 | Exemplos Windows |
|---|---|---|
| Criar/encerrar worker cooperativamente | `std::thread`; `std::jthread` e `std::stop_token` para solicitação de parada cooperativa | `CreateThread`, thread pool, handles e espera; encerra-se pedindo saída e aguardando, não terminando a thread arbitrariamente. |
| Exclusão mútua | `std::mutex`, `std::shared_mutex`, `std::unique_lock` | Critical sections, SRW locks, mutex kernel. |
| Aguardar condição | `std::condition_variable` / `_any` associada a mutex | Condition variables/SRW, eventos e funções de wait. |
| Contagem de recursos/sinalização | `std::counting_semaphore`, `std::binary_semaphore`, atomics | Semaphores, events, waitable objects. |
| Estado atômico e ordering | `std::atomic<T>`, `std::memory_order` | Interlocked functions e primitivas com contratos Windows; fences quando necessários. |
| Barreiras entre tarefas | `std::barrier` | Windows synchronization barriers ou mecanismo de alto nível. |

O modelo C++ permite escrever lógica que pode ser compilada em Windows, mas **não garante semântica correta só por substituir uma API por outra**: duração de espera, fairness, robustez contra abandono, compartilhamento entre processos, cancelamento e ordering devem ser escolhidos pelo contrato. C++20 `std::jthread` não encerra automaticamente uma operação que ignora stop token. Para APIs de wait Windows, handles de sincronização têm semânticas e tempos limite próprios. Referências: [C++ thread support](https://en.cppreference.com/w/cpp/thread), [C++ memory order](https://en.cppreference.com/w/cpp/atomic/memory_order), [Windows synchronization functions](https://learn.microsoft.com/en-us/windows/win32/sync/synchronization-functions) e [synchronization objects](https://learn.microsoft.com/en-us/windows/win32/sync/synchronization-objects).

### CPU, áudio e GPU não compartilham um único mecanismo de sincronização

- Mutexes e atomics coordenam threads de CPU segundo o modelo de memória C++/OS.
- Os modelos de ordenação da ISA também diferem: x86-64 costuma permitir menos reordenação observável do que AArch64 em operações comuns, mas isso não autoriza remover `acquire`/`release`, atomics ou barreiras da semântica guest. O tradutor precisa preservar o contrato de sincronização, não imitar apenas a tendência do hardware host.
- Um mutex não prova que comandos GPU terminaram ou que dados CPU estão visíveis à GPU.
- APIs gráficas têm fences, semaphores, barriers e estados/transições de recurso específicos. Em Vulkan, sincronização e visibilidade entre host, filas e dispositivos são explícitas; em D3D12, aplicações submetem listas a filas e coordenam seu progresso.
- Buffers mapeados, staging e upload precisam seguir o contrato da API gráfica e do recurso.
- Callback de áudio pode ocorrer em thread própria; locks que bloqueiem por tempo indeterminado no callback podem causar underrun. A interface de áudio deve definir quem produz PCM, quem consome e como a fila é sincronizada.

### Proposta para `PlatformThreading`

Expor criação de worker, join, parada cooperativa, mutex/lock, condition wait, semáforo e atomics apenas se o runtime precisar deles. Definir regras: proprietário, thread permitida para cada operação, timeout, tratamento de erro e shutdown. Encapsular sincronização GPU dentro de `PlatformGraphics`; não expor uma falsa “barreira universal”.

Para a comparação de modelos de memória e instruções, consultar as especificações de arquitetura [Arm A-profile](https://developer.arm.com/documentation/ddi0487/latest) e [Intel 64/IA-32](https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html), além das regras de atomics de [C++](https://en.cppreference.com/w/cpp/atomic/memory_order). A afirmação acima é uma orientação arquitetural geral; o mapeamento exato depende das instruções e ordens efetivamente observadas no binário, ainda desconhecido.

## 4. Relógios e temporização

### Relógio recomendado como base host

No Windows nativo, Microsoft recomenda `QueryPerformanceCounter` (QPC) para timestamp de alta resolução e intervalo curto no mesmo sistema. QPC é monotônico em relação à referência externa de horário civil; ele não é UTC. Para time-of-day sincronizado, a documentação aponta `GetSystemTimePreciseAsFileTime`. SDL2 oferece `SDL_GetPerformanceCounter()` e `SDL_GetPerformanceFrequency()` para contagem relativa de alta resolução; chamadas são convertidas em durações, não em horário de calendário.

Fontes: [Acquiring high-resolution time stamps](https://learn.microsoft.com/en-us/windows/win32/sysinfo/acquiring-high-resolution-time-stamps), [`SDL_GetPerformanceCounter`](https://wiki.libsdl.org/SDL2/SDL_GetPerformanceCounter).

### `PlatformClock` deve separar fontes temporais

- **MonotonicNow:** medir intervalos e criar deadlines sem saltos de ajuste de relógio civil.
- **WallClockNow:** timestamps para logs/telemetria; não dirigir timestep de gameplay.
- **WaitUntil/Delay:** solicitação de espera, com precisão/tolerância documentadas; `sleep_for` não garante despertar exatamente no prazo.
- **Frame pacing:** controle de apresentação separado do timestep lógico e do relógio de áudio.

Uma estratégia comum para runtime de jogo é acumular tempo monotônico, limitar `delta` e executar uma ou mais atualizações com passo lógico definido, enquanto renderização pode interpolar. Isso é uma opção arquitetural, não uma afirmação sobre o comportamento da origem. Se o programa de origem espera timers, callbacks ou cadência específicos, mudar o timestep pode alterar física, animação, áudio, repetição de input, timeouts e ordem de eventos. Adaptação de frequência não deve ser introduzida sem especificação e testes de comportamento.

`PlatformClock` não deve afirmar que refresh do monitor, target FPS, frequência de atualização do jogo e sample rate de áudio são o mesmo relógio. Se for preciso sincronizá-los, estabelecer uma política explícita, limites de drift e comportamento em pausa/perda de foco.

## 5. Entrada de dispositivos

### Comparação

| Alternativa | Pontos fortes | Limites/decisões |
|---|---|---|
| **Raw Input (Win32)** | API de baixo nível para receber dados de dispositivos registrados; pode distinguir dispositivos e dá acesso a keyboard/mouse/HID, com mensagens/buffer e notifications de alteração de dispositivo. | Aplicação interpreta relatórios HID e normaliza controles; não fornece por si uma semântica uniforme de botões de jogo para todos os modelos. Acoplamento Windows. |
| **XInput** | Interface simples para consultar state de controles compatíveis; estado inclui botões, sticks, triggers e vibração. A documentação descreve até quatro controladores. | Foco em gama de controladores compatíveis; não é API genérica para qualquer HID. Seleção de índices/dispositivos e suporte de modelos variam. |
| **SDL2 GameController/Joystick** | Mapeia dispositivos a nomes padronizados de botões/eixos, consulta estado e expõe eventos/hotplug, rumble e capacidades disponíveis. | O mapeamento depende de SDL e seus mappings/backends; capacidades devem ser consultadas e podem variar por dispositivo. Introduz dependência da biblioteca. |
| **SDL2 eventos para teclado/mouse** | Eventos de janela/input em camada que também suporta outras plataformas. | Requer loop/processamento de eventos; comportamento de captura/raw relativo a plataforma deve ser configurado e validado. |

Microsoft documenta Raw Input e notificação `WM_INPUT_DEVICE_CHANGE` em [Raw Input](https://learn.microsoft.com/en-us/windows/win32/inputdev/raw-input) e consulta de estado, até quatro controles compatíveis, dead zones e vibração em [Getting Started with XInput](https://learn.microsoft.com/en-us/windows/win32/xinput/getting-started-with-xinput). SDL2 lista operações de mapping, estado de eixos/botões, attached status, player index e rumble em [Game Controller](https://wiki.libsdl.org/SDL2/CategoryGameController).

### Proposta para `PlatformInput`

Publicar eventos normalizados (`DeviceConnected`, `DeviceDisconnected`, `ButtonChanged`, `AxisChanged`, `PointerMoved`) e snapshots por dispositivo. Preservar identificador estável sempre que a API permitir; não assumir que o índice de porta é identidade persistente.

Normalização deve explicitar:

- intervalos e escala de eixos;
- dead zone axial/radial, curva e trigger threshold;
- estado pressed/released e tratamento de eventos perdidos;
- repetição de teclado distinta de transição física;
- rumble como capacidade opcional com duração/intensidade e falha explícita;
- hotplug e remoção durante polling;
- foco/captura de mouse e perda de foco.

Input não deve chamar diretamente gameplay nem manter a política de remapeamento do título: expõe estado/eventos e deixa o runtime converter para a semântica lógica do programa.

## 6. Áudio

### WASAPI versus SDL2

| Aspecto | WASAPI | SDL2 Audio |
|---|---|---|
| Escopo | API Windows para streams entre aplicação e endpoint; modos shared/exclusive conforme inicialização. | API de biblioteca com backend selecionado pela SDL, destinada a múltiplos sistemas. |
| Buffer | Aplicação obtém/fornece dados ao endpoint conforme interfaces e modo. `IAudioClient3` expõe periodicidades compatíveis e inicialização de stream shared com periodicidade especificada. | `SDL_OpenAudioDevice()` negocia `desired` e `obtained`; callback recebe buffer ou a aplicação enfileira áudio. |
| Thread/callback | A arquitetura é baseada em clients/serviços e buffers de endpoint; o app coordena o fluxo. | Callback executa em thread separada na maioria dos casos, segundo a documentação SDL2; o tamanho solicitado pode ser ajustado pelo driver. |
| Latência/formato | Acesso mais direto ao mecanismo do Windows e periodicidades, mas requer lidar com formato, endpoint, erro/dispositivo invalidado e ciclo de buffers. | API uniforme e negociação de formato; buffer menor reduz latência, mas aumenta risco de underrun quando processamento não acompanha. |
| Portabilidade | Windows. | Maior portabilidade, com comportamento de backends sujeito à plataforma/driver. |

WASAPI define `IAudioClient`, `IAudioRenderClient`, endpoint buffers e `IAudioClock`; o engine shared mistura streams. Veja [About WASAPI](https://learn.microsoft.com/en-us/windows/win32/coreaudio/wasapi). SDL2 documenta negociação de formato, tamanho de buffer em sample frames e callback/fila em [`SDL_OpenAudioDevice`](https://wiki.libsdl.org/SDL2/SDL_OpenAudioDevice) e [`SDL_AudioSpec`](https://wiki.libsdl.org/SDL2/SDL_AudioSpec).

### Proposta para `PlatformAudio`

Separar:

1. **Mixer/runtime:** recebe sons ou PCM, aplica mixagem e gera blocos PCM.
2. **Fila de saída:** gere buffers e under/overflow sem bloquear a thread de áudio.
3. **Backend:** converte/rejeita formatos e interage com WASAPI ou com SDL.
4. **Clock/status:** observa posição/taxa de consumo quando o backend oferece isso; não assume sincronização com FPS.

Negociar explicitamente sample rate, canais, formato, tamanho/periodicidade, modo compartilhado/exclusivo e política de conversão. A dimensão de latência depende do tamanho e do scheduling: baixar buffer sem medir capacidade de produção pode piorar estalos/dropouts. Recuperação de device invalidated e encerramento limpo fazem parte da interface.

## 7. GPU e apresentação

### API gráfica não é a camada inteira de plataforma

Uma API gráfica descreve dispositivos, recursos, shaders, comandos e apresentação. Ela não substitui por si só:

- criação de janela e tratamento de eventos do sistema;
- relógio geral, threads de gameplay ou input;
- gerenciamento global de memória do processo;
- semântica de áudio.

O módulo gráfico pode depender de uma janela/surface fornecida por `PlatformWindow` ou pela SDL, de um adapter/device, do driver e da camada de apresentação do SO.

`PlatformWindow` aparece aqui como dependência de integração necessária à apresentação, não como um dos seis módulos propostos nesta etapa. Pode ser implementado por Win32 ou fornecido junto a uma biblioteca multiplataforma como SDL.

### Comparação de APIs

| API | Modelo documentado | Implicações para a camada proposta |
|---|---|---|
| **Direct3D 11** | `ID3D11Device` cria recursos/capacidades; immediate context emite comandos; deferred context grava command lists, sem herdar estado do immediate context. | Estado gráfico é gerido pela API; é possível começar com um caminho de rendering mais direto. Threads e contexts ainda precisam seguir regras da API. |
| **Direct3D 12** | Aplicação grava command lists e submete a uma ou mais command queues; não há immediate context associado ao device como em D3D11. | Aplicação assume mais gestão de scheduling e sincronização; fences/vida útil de recursos tornam-se responsabilidade explícita do renderer. |
| **Vulkan** | API explícita e cross-platform; dependências de execução/memória, filas e visibilidade de recursos precisam ser especificadas. WSI é fornecida por extensões, incluindo integrações por plataforma. | Mais controle, mas maior responsabilidade e superfície de código. Não é uma abstração de OS universal: surface/presentation tem partes específicas da plataforma. |

Fontes: [D3D11 Device introduction](https://learn.microsoft.com/en-us/windows/win32/direct3d11/overviews-direct3d-11-devices-intro), [D3D12 command queues and lists](https://learn.microsoft.com/en-us/windows/win32/direct3d12/command-queues-and-command-lists), [Vulkan synchronization](https://docs.vulkan.org/spec/latest/chapters/synchronization.html), [Vulkan WSI](https://docs.vulkan.org/spec/latest/chapters/VK_KHR_surface/wsi.html).

### Swap chain, VSync e GPU

No caminho Direct3D/DXGI, a swap chain mantém buffers de apresentação e `Present` solicita a exibição; flip model compartilha back buffers com o compositor e tem políticas/estatísticas próprias. VSync/intervalo de apresentação é decisão de configuração e do caminho usado, não propriedade implícita de toda API. Veja [DXGI flip model](https://learn.microsoft.com/en-us/windows/win32/direct3ddxgi/dxgi-flip-model).

O sincronismo CPU↔GPU precisa garantir que o CPU não sobrescreva recurso ainda lido, que upload seja visível e que recurso não seja destruído enquanto está em uso. Em Vulkan esses pontos são explicitamente coordenados por fences, semaphores e barriers; D3D12 também expõe filas/listas e sincronização explícita. Em D3D11 parte da gestão é feita pela API/driver, mas a aplicação continua obrigada a respeitar usos e vida útil dos recursos.

### Proposta para `PlatformGraphics`

Uma interface superior independente de API pode expor:

- inicializar/destruir device e surface;
- criar/destruir buffers, texturas, samplers e pipelines;
- transferir dados e expressar estados/uso dos recursos;
- gravar/submeter trabalho de desenho;
- apresentar e informar resize, device loss e status;
- configurar política de VSync/frame latency como capacidade configurável;
- sincronizar CPU/GPU por objetos do backend sem os expor ao gameplay.

Se a origem exigir diretamente uma semântica de GPU não mapeável ao modelo escolhido, será necessário um emulador/tradutor de comandos específico; `PlatformGraphics` sozinho não transforma automaticamente uma API de origem em Direct3D.

## 8. Responsabilidades e dependências entre módulos

| Módulo conceitual | Responsabilidades | Depende de | Não deve assumir |
|---|---|---|---|
| `PlatformMemory` | Heap, reserva/commit/proteção, alinhamento, regiões guest, compartilhamento explicitamente pedido. | Windows virtual memory; allocator C++/runtime. | Endereço físico fixo, memória de GPU diretamente acessível ou permissões executáveis automáticas. |
| `PlatformThreading` | Workers, join, cancelamento cooperativo, mutex/condvar/semaphore e contratos de ordenação CPU. | C++20 ou Win32. | Que mutex CPU sincroniza GPU ou que cancelar significa encerrar thread à força. |
| `PlatformClock` | Contador monotônico, conversão a duração, deadlines, espera e métricas. | QPC ou SDL performance counter. | UTC, precisão de sleep, refresh = FPS lógico ou relógios de áudio/GPU idênticos. |
| `PlatformInput` | Enumerar dispositivos, eventos/snapshots, normalizar botões/eixos, hotplug e capabilities. | Raw Input, XInput ou SDL2. | Layout fixo de gamepad, índices estáveis ou suporte garantido a rumble. |
| `PlatformAudio` | Negociar device/formato, alimentar buffers, controlar stream, status/erro, recuperação. | WASAPI ou SDL2; fila/mixer interno. | Callback no mesmo thread do gameplay, latência zero ou sample rate fixo sem negociação. |
| `PlatformGraphics` | Device/surface, recursos/shaders, submissão, apresentação e sincronização CPU/GPU. | D3D11/12 ou Vulkan + DXGI/WSI/janela. | Que API gráfica abstrai o sistema operacional ou que a GPU consome imediatamente dados CPU. |

Dependências recomendadas:

```text
CompatibilityRuntime ── PlatformMemory
        │               PlatformThreading
        ├─────────────── PlatformClock
        ├─────────────── PlatformInput
        ├─────────────── PlatformAudio
        └─────────────── PlatformGraphics
                            │
                      PlatformWindow
```

`PlatformAudio` pode usar `PlatformThreading` para worker/fila e `PlatformClock` para métricas, mas não deveria compartilhar o lock do gameplay com callback de áudio. `PlatformGraphics` precisa da janela/surface e de primitivas próprias de sincronização GPU. `PlatformInput` normalmente consome eventos da janela e produz estados para o runtime; não deve depender do renderer. `PlatformMemory` é utilitário transversal, mas o renderer pode precisar de alocadores de upload específicos que permanecem propriedade do módulo gráfico.

## 9. Riscos, limitações e fatos ainda desconhecidos

### Riscos técnicos

- **ABI e layout:** se o código recompilado mantém chamadas ou estruturas de origem, pontes de ABI têm de traduzir parâmetros, retorno, alinhamento, unwind e ownership.
- **Tempo:** alterar frequência lógica, acumular/clamp `delta` ou mudar pausa pode afetar determinismo, física, áudio e timers.
- **Input:** abstração pode perder dados específicos do dispositivo, report rate, identidade ou recursos avançados.
- **Áudio:** incompatibilidade de PCM, reamostragem, fila insuficiente e callback bloqueado podem gerar latência/underrun.
- **GPU:** APIs diferem em estado implícito/explícito, lifetime, pipeline, sincronização e apresentação; escolher uma exige decidir nível de emulação.
- **Performance:** abstração com cópia, conversão ou lock por operação pode ser cara. Otimizar antes de definir semântica e medir pode introduzir divergências.
- **Device loss/resize/hotplug:** são eventos normais a tratar, não exceções impossíveis.

### Desconhecidos para a origem

Ainda não se sabe, e este documento não infere:

- ISA, sistema operacional ou ABI do executável de origem;
- se usa memória fixa, aliases, MMIO, código dinâmico ou permissões executáveis;
- número/semântica de threads e primitivas de sincronização;
- fonte temporal, frequência lógica, frame pacing ou dependência de relógios físicos;
- mapeamento de input, controles proprietários, vibração ou hotplug esperado;
- formato de áudio, sample rate, latência, mixer e modelo de dispositivo;
- API gráfica, formato de comandos, shader language, apresentação e comportamento de sync;
- se existe representação de alto nível dos subsistemas ou se será necessário emular interfaces de baixo nível.

### Conclusão

Para Windows 11 x64, C++20 pode fornecer a base portável para grande parte de threads, atomics e gestão de recursos em user mode; Win32 fornece controles de sistema como memória virtual, input e integração nativa, enquanto SDL2 oferece uma alternativa multiplataforma para input, áudio, janela e relógios. WASAPI, D3D11/12 e Vulkan tratam de domínios distintos e têm contratos diferentes.

A arquitetura proposta deve começar com interfaces que expressem semântica e ownership, e adaptadores substituíveis. A escolha entre SDL2 e APIs nativas, e entre D3D11, D3D12 ou Vulkan, permanece decisão em aberto até se conhecerem os requisitos da origem. Nenhuma escolha aqui prova compatibilidade com o remake.

## Referências técnicas públicas

### Memória e sincronização

- Microsoft, [Virtual Address Space](https://learn.microsoft.com/en-us/windows/win32/memory/virtual-address-space)
- Microsoft, [Memory Protection](https://learn.microsoft.com/en-us/windows/win32/memory/memory-protection)
- Microsoft, [`VirtualAlloc`](https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-virtualalloc)
- Microsoft, [Synchronization Functions](https://learn.microsoft.com/en-us/windows/win32/sync/synchronization-functions) e [Synchronization Objects](https://learn.microsoft.com/en-us/windows/win32/sync/synchronization-objects)
- cppreference, [C++ thread support](https://en.cppreference.com/w/cpp/thread) e [atomic memory order](https://en.cppreference.com/w/cpp/atomic/memory_order)

### Temporização e input

- Microsoft, [Acquiring high-resolution time stamps](https://learn.microsoft.com/en-us/windows/win32/sysinfo/acquiring-high-resolution-time-stamps)
- SDL2 Wiki, [`SDL_GetPerformanceCounter`](https://wiki.libsdl.org/SDL2/SDL_GetPerformanceCounter)
- Microsoft, [Raw Input](https://learn.microsoft.com/en-us/windows/win32/inputdev/raw-input)
- Microsoft, [Getting Started with XInput](https://learn.microsoft.com/en-us/windows/win32/xinput/getting-started-with-xinput)
- SDL2 Wiki, [Game Controller API](https://wiki.libsdl.org/SDL2/CategoryGameController)

### Áudio e gráficos

- Microsoft, [About WASAPI](https://learn.microsoft.com/en-us/windows/win32/coreaudio/wasapi)
- SDL2 Wiki, [`SDL_OpenAudioDevice`](https://wiki.libsdl.org/SDL2/SDL_OpenAudioDevice) e [`SDL_AudioSpec`](https://wiki.libsdl.org/SDL2/SDL_AudioSpec)
- Microsoft, [Introduction to a Device in Direct3D 11](https://learn.microsoft.com/en-us/windows/win32/direct3d11/overviews-direct3d-11-devices-intro)
- Microsoft, [Work submission in Direct3D 12](https://learn.microsoft.com/en-us/windows/win32/direct3d12/command-queues-and-command-lists)
- Microsoft, [DXGI flip model](https://learn.microsoft.com/en-us/windows/win32/direct3ddxgi/dxgi-flip-model)
- Khronos Group, [Vulkan Synchronization](https://docs.vulkan.org/spec/latest/chapters/synchronization.html) e [Window System Integration](https://docs.vulkan.org/spec/latest/chapters/VK_KHR_surface/wsi.html)
