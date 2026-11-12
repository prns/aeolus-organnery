# Windows port: engineering plan

Target: a standalone 64-bit Windows application for Windows 10 and 11. This is not a VST/AU plugin. The port should preserve Aeolus's synthesiser, instrument data, MIDI mapping, presets, and the existing organ-console workflow as closely as practical.

## Repository findings

The current source tree is a Linux-first application, not a portable Windows project:

- `source/Makefile` builds the engine and two dynamically loaded UI modules. It links JACK, zita-alsa-pcmi, clthreads, and (for the X11 UI) clxclient, Xft, and X11.
- `source/audio.h` exposes a JACK-based audio path and conditionally includes the Linux ALSA PCM implementation. `source/audio.cc` mixes synthesis processing with JACK/ALSA device setup and callbacks.
- `source/imidi.cc` is implemented against the ALSA sequencer API. `imidi.h` has an Apple CoreMIDI declaration, but the implementation is not a complete CoreMIDI backend and there is no Windows MIDI backend.
- `source/main.cc` uses POSIX facilities (including `unistd.h`, `sys/mman.h`, `dlfcn.h`, signals, and `getopt`) and loads a UI shared object at runtime.
- The existing graphical interface is X11/clxclient-based. Those dependencies are not a drop-in Windows GUI solution.
- The model, audio engine, MIDI service, and UI all use `clthreads` thread/event primitives. A Windows port therefore needs a supported Windows build of that dependency or a deliberate replacement/adapter; simply excluding JACK and ALSA is not sufficient.

The Makefile identifies Aeolus as version 0.9.10. Source files carry GPL-3.0-or-later notices; preserve those notices in any derived source.

## Proposed Windows architecture

Keep the synthesis/model code independent of the operating system. Put platform services behind narrow interfaces:

1. **Audio host**
   - Retain the current JACK and Linux ALSA implementations behind the existing platform guards.
   - Add a Windows audio host with WASAPI shared/exclusive modes.
   - Add ASIO as an optional backend. Its SDK/toolchain requirements and redistribution terms must be reviewed separately; do not bundle an SDK without checking its licence.
   - Keep the audio callback real-time safe: no allocation, file I/O, locks with unbounded waits, or GUI work in the callback.
   - Support stereo first; preserve the engine's existing multi-channel output capability where the selected device supports it.

2. **MIDI host**
   - Add Windows MIDI input/output enumeration and event delivery for USB-MIDI devices.
   - Preserve Aeolus's channel map, note handling, controller semantics, and stop/preset control messages.
   - Keep device callbacks small and forward events through the existing queues.

3. **Graphical interface**
   - The current X11/clxclient UI cannot be compiled unchanged on Windows.
   - Port its windows, controls, drawing, keyboard/mouse interaction, and layout to a Windows-capable UI layer while retaining the current visual arrangement and interaction model.
   - Do not replace the console with a generic plugin interface. The deliverable remains a normal standalone `.exe`.

4. **Application/platform layer**
   - Replace POSIX-only startup, dynamic loading, command-line parsing, signals, memory locking, and path handling with portable wrappers or Windows implementations.
   - Resolve bundled resources (instrument definitions, presets, and generated wave tables) relative to the installation/application data directory, not the current working directory.
   - Keep user-writable presets and generated data in an appropriate per-user directory.

5. **Build and packaging**
   - Introduce a Windows build configuration (CMake is the proposed cross-platform build generator) without breaking the existing Linux Makefile.
   - Build x64 Release and Debug configurations with MSVC; document any optional MinGW path only after it is tested.
   - Package the standalone executable, required runtime libraries, and the Aeolus instrument/data files. No installer or code signing is assumed at this stage.

## Implementation milestones

- [ ] **P0 — Baseline and build inventory:** record the complete source/dependency inventory, licence notices, data-file layout, and a reproducible Linux baseline.
- [ ] **P1 — Portability boundary:** isolate OS-specific startup, threading, audio, MIDI, filesystem, and UI code. Keep Linux builds working.
- [ ] **P2 — Windows build skeleton:** add a Windows x64 project/build configuration and compile the portable synthesis/model subset.
- [ ] **P3 — Audio:** implement and test WASAPI; then add optional ASIO and verify device changes, sample rates, buffer sizes, channel counts, and shutdown.
- [ ] **P4 — MIDI:** implement USB-MIDI input/output, hot-plug/device selection, and regression tests for notes, sustain, controllers, and channel mapping.
- [ ] **P5 — GUI:** port the existing organ console and its editing/settings windows, matching layout and behaviour.
- [ ] **P6 — Packaging and QA:** bundle data/runtime dependencies and test on clean Windows 10 and Windows 11 x64 systems.

## Acceptance criteria

- Starts as a normal Windows application (`Aeolus.exe`), with no Linux subsystem, JACK server, or plugin host required.
- WASAPI works on a clean Windows installation; ASIO is available when its optional driver/runtime is installed.
- USB-MIDI devices can be selected and used for playing notes and controlling stops/presets.
- The original instrument definitions, tuning/temperaments, presets, synthesis editor, and organ-console workflow remain available.
- No audio-thread glitches caused by UI/device operations; clean start/stop and device-loss handling.
- Existing Linux build remains unaffected by Windows-specific changes.

## Current status

This file records the initial technical assessment and intended sequence only. No Windows backend, GUI port, executable, or passing Windows build has been produced yet. Each milestone should be implemented and tested in its own commits on the `windows-port` branch.

## Work log

### 2026-11-12 — Windows byte-order portability

- Updated `source/global.h` to use project-local `AEOLUS_*` byte-order macros.
- Windows builds now select little-endian serialization without relying on the non-portable `<endian.h>` header.
- Avoided defining implementation-reserved double-underscore byte-order macros, which can collide with toolchain/system headers.
- This is a narrow source portability fix only. It has not yet been compiled with MSVC or MinGW and does not establish that the application builds on Windows.

### Next engineering steps

1. Inspect and choose the Windows-compatible thread/event dependency or implement a small compatibility layer for the `clthreads` API used throughout the engine.
2. Separate the synthesis processing loop from the JACK/ALSA device setup in `Audio`, then add the WASAPI host.
3. Replace the ALSA-only `Imidi` implementation with a platform MIDI service; preserve the existing queue/message protocol so the model remains unchanged.
4. Only after the service boundaries are stable, introduce the Windows application entry point and GUI host.

### Source inventory clarification

The repository also contains files named `audiowin.cc/.h` and `midiwin.cc/.h`. These are **Aeolus GUI windows** for audio and MIDI settings, implemented using `clxclient` and X11; they are not Windows audio or Windows MIDI backends.

### 2026-11-12 — Remove unconditional ALSA dependency from shared messages

- Guarded the ALSA header include in `source/messages.h` so non-Linux compilation of shared model/message declarations does not require ALSA development headers.
- Kept the Linux `snd_seq_t` fields unchanged under `__linux__`; non-Linux message layouts now expose opaque backend pointers instead.
- Guarded the ALSA sequencer transmit implementation in `source/model.cc`. Non-Linux builds retain the model-facing MIDI transmit methods as temporary no-op stubs until the native Windows MIDI service is connected.
- This is an intermediate portability boundary, not functional Windows MIDI support. The Windows MIDI backend must replace these stubs before the port can be considered feature-complete.
- No Windows compiler or build environment is available in this repository action, so compilation remains unverified.
