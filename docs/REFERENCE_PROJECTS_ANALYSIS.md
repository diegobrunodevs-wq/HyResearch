# Hyrule Research --- Reference Projects Analysis

**Status:** Initial static analysis\
**Date:** 2026-11-05\
**Scope:** Uploaded source archives only; no projects were built or
executed.

## 1. Purpose

This document records what the three supplied repositories demonstrate,
what they do not demonstrate, and how they can inform Hyrule Research.

The analysis distinguishes repository statements and inspected project
structure from research hypotheses. A repository's README or source tree
is evidence of its stated design, not proof of complete compatibility or
correctness.

## 2. Repositories examined

  ------------------------------------------------------------------------
  Repository/archive       Observed snapshot       Primary role in this
                                                   research
  ------------------------ ----------------------- -----------------------
  `oot-main.zip`           Archive root identifies Reconstructed
  (`zeldaret/oot`)         commit `14759ab9...`    source-code research
                                                   for the original
                                                   Nintendo 64 game

  `Shipwright-9.3.1.zip`   Archive root identifies Native PC adaptation
                           commit `f0599f96...`    based on the OOT
                                                   decompilation

  `Lichii-nx2.zip`         Archive root identifies Experimental
                           commit `27226d25...`    Switch/Switch 2
                                                   emulator codebase for
                                                   studying platform
                                                   behavior and
                                                   compatibility layers
  ------------------------------------------------------------------------

A second Shipwright archive, `Shipwright-9.3.1.tar.gz`, was also
supplied. This initial report uses the ZIP snapshot for its inventory
and README/CMake inspection; the two archives should be hash-compared
before assuming they are byte-identical.

## 3. zeldaret/oot

### What the repository states

Its README explicitly describes the project as a work-in-progress
decompilation of *The Legend of Zelda: Ocarina of Time*. Its purpose is
to recreate a source-code base from information in the game, using
static and/or dynamic analysis. The README explicitly says it is **not
producing a PC port**.

The README lists supported historical targets including Nintendo 64
releases, GameCube variants and iQue Player. It also states that the
repository does not include the assets needed to build the ROM; a
compatible prior game copy is required to extract those assets.

### Technical significance

-   Provides reconstructed C source and headers for studying original
    game systems.
-   Includes MIPS-oriented build tooling and documentation.
-   Offers a useful reference for understanding the original game's
    logic and historical platform assumptions.
-   Demonstrates the distinction between decompilation/reconstruction
    and a native PC port.

### Limitations for Hyrule Research

-   It is not the Switch 2 remake's source code.
-   It does not itself provide a Windows-native port.
-   Its reconstructed code and assets cannot be assumed to match a later
    remake.
-   A successful ROM rebuild is not equivalent to a native PC
    executable.

**Research use:** study code organization, game systems,
platform-specific interfaces and how reconstruction projects validate
matching behavior. Do not treat it as a direct codebase for the Switch 2
remake.

## 4. Shipwright / Ship of Harkinian

### What the repository states

The supplied README describes Ship of Harkinian as being built on
`libultraship` (LUS), a custom library designed to mimic functionality
of the Nintendo 64 `libultra` SDK on modern hardware. It also states
that the project depends on the OOT decompilation source.

The README says copyrighted game assets are not included. A legally
acquired compatible game copy is required; assets are extracted and
reformatted into `.o2r` archives used by the program.

The README identifies these graphics APIs: - DirectX 11 on Windows; -
OpenGL across platforms; - Metal on macOS.

The supplied root CMake file sets C and C++ language standards,
identifies Windows platform architecture, and contains logic for x64,
Win32 and ARM64. When Windows ARM64 is selected, it chooses ARM64 vcpkg
triplets. This is evidence that the supplied build configuration
contains an ARM64 path; it is not evidence that every target or
dependency has been successfully validated on ARM64.

### Technical significance

-   Demonstrates a practical native-PC adaptation of the original OOT
    decompilation.
-   Shows the importance of compatibility layers for replacing platform
    SDK behavior.
-   Shows a separation between game logic, platform services, rendering
    backends and asset packaging.
-   Provides a concrete reference for modern graphics API integration
    and desktop input/audio/runtime support.

### Limitations for Hyrule Research

-   It targets the original OOT code lineage, not the Switch 2 remake.
-   Its compatibility layer is tailored to the N64/libultra environment;
    it is not a general-purpose Switch 2 compatibility layer.
-   The supplied project requires user-provided game assets and has its
    own build dependencies.
-   Presence of ARM64 configuration in CMake does not prove the project
    can be built or run in every ARM64 environment.

**Research use:** inspect the design of `libultraship`, platform
abstraction boundaries, renderer selection, input/audio handling, asset
archive loading and build configuration. Use it as a porting case study,
not as a ready-made Switch 2 port foundation.

## 5. Lichii NX2

### What the repository states

The supplied README describes Lichii as an experimental, open-source
Nintendo Switch and Switch 2 emulator written in C++. It says it builds
on Eglantine, Eden, yuzu and Sudachi, and that compatibility/performance
vary by game, hardware and driver. It warns that rendering errors,
stutters and crashes remain possible and that passing automated tests
does not prove correct gameplay rendering.

The README says games, keys and firmware are not included and directs
users to use only legally obtained game and system files. It also
identifies inherited licensing and copyright notices.

The CMake project identifies itself historically as `yuzu`, uses
platform and architecture detection modules, and includes separate
source areas for core emulation, audio, video, HID/input, frontend and
shader recompilation. The source tree contains distinct OpenGL and
Vulkan renderer implementations, as well as shader-related components.

### Technical significance

An emulator is useful to study because it must model or translate
behavior expected by software designed for another hardware/software
environment. The source tree gives us concrete areas to investigate:

-   CPU/core execution and system interfaces;
-   memory and address-space behavior;
-   GPU command processing and rendering;
-   shader translation/recompilation;
-   audio processing;
-   HID/input;
-   operating-system services and frontend integration.

### Limitations for Hyrule Research

-   An emulator is not a native recompilation of one particular game.
-   Its presence does not establish that the supplied build fully
    supports Switch 2 or any specific title.
-   The README explicitly characterizes the project as experimental and
    warns that compatibility varies.
-   Emulator implementation details cannot be assumed to describe the
    exact internals of the Switch 2 remake or its engine.
-   It does not provide the remake's source code or game assets.

**Research use:** inspect architecture boundaries and compatibility
techniques at a conceptual/source level. Do not assume the emulator's
internal implementation is a specification of the console.

## 6. Comparison

  --------------------------------------------------------------------------
  Question             zeldaret/oot      Shipwright        Lichii NX2
  -------------------- ----------------- ----------------- -----------------
  Reconstructed        Yes, project goal Uses OOT          No
  original OOT logic                     decompilation     

  Native PC execution  Not its goal      Yes, project goal No; emulator
  of OOT                                                   architecture

  Switch/Switch 2      No direct focus   No direct focus   Yes, as an
  environment study                                        experimental
                                                           emulator

  Rendering/platform   Limited, original Strong reference  Strong reference,
  abstraction lessons  platform focus                      emulator context

  Direct source for    No                No                No
  Switch 2 remake                                          

  Ready-made base for  No                No                No
  our target port                                          
  --------------------------------------------------------------------------

## 7. What this means for Hyrule Research

These repositories provide three different kinds of evidence:

1.  **Game reconstruction:** zeldaret/oot shows how original game code
    can be reconstructed and checked against historical builds.
2.  **Native porting:** Shipwright shows how reconstructed game logic
    can be adapted to modern desktop systems using compatibility layers
    and renderer backends.
3.  **Platform emulation:** Lichii provides a large codebase in which to
    study CPU, memory, GPU, shader, audio, input and system-service
    compatibility.

Together they are a valuable study set. They do **not** close the
largest gap in our target project: we still lack an authorized technical
base for the Switch 2 remake itself, and we have not established that
its architecture, engine, assets or internal systems correspond to the
original OOT implementation.

## 8. Recommended investigation order

### Phase A --- Shipwright (first)

Inspect, without building or modifying: 1. `libultraship` boundaries and
interfaces; 2. platform-specific code and abstractions; 3. graphics
backend selection; 4. input and audio subsystems; 5. asset
extraction/packaging and `.o2r` loading; 6. Windows build configuration
and dependencies.

Goal: document how the original OOT code is made to run as a native
desktop application.

### Phase B --- zeldaret/oot

Inspect: 1. game module organization; 2. actor and gameplay systems; 3.
platform-dependent interfaces; 4. MIPS-specific code and build process;
5. how assets are extracted and how rebuild correctness is checked.

Goal: understand the original game's reconstructed logic and the
boundary between game logic and hardware-specific code.

### Phase C --- Lichii NX2

Inspect: 1. CPU/core execution boundary; 2. memory subsystem; 3. GPU
command processing and renderer architecture; 4. shader
translation/recompilation; 5. audio, HID and system-service layers; 6.
platform detection and build targets.

Goal: learn how an emulator models a complex platform, while keeping
clear that emulation and native game recompilation are different
strategies.

### Phase D --- Synthesis

Create an architecture map for Hyrule Research that separates: - game
logic; - CPU instruction/ABI concerns; - operating-system and SDK
interfaces; - graphics and shader pipeline; - audio; - input; - memory
and resource formats; - build and validation pipeline.

Only after these studies should we choose a concrete prototype.

## 9. Guardrails

-   Do not assume the Switch 2 remake is a simple asset replacement or
    direct continuation of the N64 code.
-   Do not assume an emulator's code reveals the exact source
    implementation of the console or game.
-   Do not copy code across repositories without reviewing each
    applicable license and attribution requirements.
-   Do not acquire or use unauthorized ROMs, firmware, keys or protected
    assets.
-   Keep research experiments independent and reproducible.
-   Record repository commit identifiers and compiler/tool versions for
    every technical result.
-   Treat static source inspection, successful compilation, runtime
    tests and gameplay compatibility as separate levels of evidence.

## 10. Initial conclusion

The strongest immediate reference for learning native port architecture
is **Shipwright**. The strongest reference for understanding the
original OOT game logic is **zeldaret/oot**. Lichii is a valuable but
different reference for platform emulation, especially GPU/shader and
system compatibility.

The next practical task is a focused Shipwright architecture
walkthrough, beginning with `libultraship` and the platform/rendering
boundary. No code changes to Hyrule Research are required for that
study.
