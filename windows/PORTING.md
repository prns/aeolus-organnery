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

1. Build and test the `clthreads` dependency for x64 Windows; see `windows/THREADING.md` for the API inventory and proposed MinGW-w64 POSIX route.
2. Separate the synthesis processing loop from the JACK/ALSA device setup in `Audio`, then add the WASAPI host.
3. Replace the ALSA-only `Imidi` implementation with a WinMM USB-MIDI service; preserve the existing queue/message protocol so the model remains unchanged.
4. Only after the service boundaries are stable, introduce the Windows application entry point and native GUI host.

### Source inventory clarification

The repository also contains files named `audiowin.cc/.h` and `midiwin.cc/.h`. These are **Aeolus GUI windows** for audio and MIDI settings, implemented using `clxclient` and X11; they are not Windows audio or Windows MIDI backends.

### 2026-11-12 — Remove unconditional ALSA dependency from shared messages

- Guarded the ALSA header include in `source/messages.h` so non-Linux compilation of shared model/message declarations does not require ALSA development headers.
- Kept the Linux `snd_seq_t` fields unchanged under `__linux__`; non-Linux message layouts now expose opaque backend pointers instead.
- Guarded the ALSA sequencer transmit implementation in `source/model.cc`. Non-Linux builds retain the model-facing MIDI transmit methods as temporary no-op stubs until the native Windows MIDI service is connected.
- This is an intermediate portability boundary, not functional Windows MIDI support. The Windows MIDI backend must replace these stubs before the port can be considered feature-complete.
- No Windows compiler or build environment is available in this repository action, so compilation remains unverified.


### 2026-11-12 — Threading API inventory and MIDI handoff safety

- Inspected the public `clthreads.h` API and recorded the concrete `P_thread`, `A_thread`, `ITC_ctrl`, mutex/condition-variable, event, and message-queue requirements in `windows/THREADING.md`.
- Identified MinGW-w64 with the POSIX thread runtime as the smallest initial Windows proof-of-concept route; this is a proposal, not a verified build.
- Initialized the non-Linux opaque MIDI backend handoff in `Model::init_iface()` to null. This prevents an uninitialized pointer from being passed to the GUI/service boundary while a Windows MIDI implementation is still pending.
- The ALSA-only `Imidi` implementation and remaining POSIX/JACK dependencies are still blockers. No Windows executable or successful Windows build has been produced.


### 2026-11-12 — Extract host-independent audio block renderer

- Added `Audio::render_block(float **outputs, int nframes)` as a device-API-independent entry point for one audio block. It consumes the existing note/command queues, updates keyboard state, renders the synth into caller-owned output buffers, and processes engine messages.
- Routed both the JACK callback and the ALSA playback loop through this shared method. JACK MIDI remains handled by the existing JACK event path inside the synthesis loop; a future WASAPI host can call the same renderer with its own output buffers and no JACK client.
- This is the first real audio-host seam, not a WASAPI implementation. `audio.h` still includes JACK headers, the JACK MIDI parser is still JACK-specific, and device lifecycle/sample-rate negotiation remain to be separated. No Windows build/test has been run.


### 2026-11-12 — Isolate JACK-specific audio host code

- Wrapped the JACK client declarations, fields, MIDI parser, callback/lifecycle methods, and JACK headers in `#ifndef _WIN32` guards.
- Kept the shared synthesis renderer and the Linux JACK/ALSA implementations available. On Windows, `audio.h` no longer requires JACK types and `audio.cc` no longer compiles the JACK host implementation or JACK MIDI event parser.
- This removes one direct audio-host dependency from the Windows compilation path, but does not yet provide a Windows device initializer, sample-rate/buffer negotiation, output-channel setup, or a complete Windows build. The current `Audio` class still needs a Windows host entry point that initializes the synthesis engine before calling `render_block()`.
- Linux behaviour has not been runtime-tested in this environment; no Windows compiler/build was run.


### 2026-11-12 — Host-managed audio format initialization

- Added public `Audio::init_host(fsamp, fsize, nplay, bform)`. It validates a non-zero sample rate and block size and accepts the existing two-channel or four-channel render layouts, sets the engine's host format, and initializes the reverb/sections through the same `init_audio()` path used by Linux backends.
- It deliberately does not allocate output buffers, start a thread, or open a device: a host such as WASAPI owns those responsibilities and supplies its output buffers to `render_block()`.
- This is an engine setup seam, not a WASAPI implementation. There is not yet a Windows device lifecycle, callback/thread, device enumeration, sample-rate negotiation, or USB-MIDI input backend. No Windows build or runtime test has been performed.


### 2026-11-12 — Validate host channel topology

- Tightened `Audio::init_host()` so the channel count must match the selected render topology: stereo requires exactly 2 outputs; four-channel B-format requires exactly 4. This prevents the B-format renderer from writing to channels that the host did not supply.
- Host setup still does not open or start a device. A future WASAPI adapter must request a compatible 2- or 4-channel float output layout, or add an explicit channel-mapping/conversion stage. It must also respect the synthesizer's fixed internal `PERIOD` processing quantum; arbitrary WASAPI packet sizes cannot yet be assumed safe.
- No compiler or runtime test was run for this change.


### 2026-11-12 — Enforce the synthesizer processing quantum

- The renderer's internal `PERIOD` is 64 frames (`source/rankwave.h`). `proc_synth()` always computes complete 64-frame blocks and writes a complete block to each output pointer. A host block that is not a multiple of 64 would therefore make the current render path unsafe (potentially writing beyond the host buffer).
- `Audio::init_host()` now rejects a requested host block size that is not a multiple of `PERIOD`, in addition to checking sample rate and matching stereo/four-channel output topology.
- WASAPI integration must negotiate a stable callback packet size that is a multiple of 64 frames (for example through the supported shared-mode engine-period API where available), or the renderer must first be redesigned to handle partial quanta with correct MIDI timestamp continuity. Do not simply pad every callback: that would alter the effective audio timeline.
- This is a defensive validation, not proof of a compatible WASAPI endpoint. No Windows build or device test has been performed.


### 2026-11-12 — First automated Windows x64 compile job

- Added `.github/workflows/windows-core.yml`, a GitHub Actions job on `windows-latest` using the MSYS2 UCRT64 x64 GCC toolchain.
- The job compiles nine synthesis/support translation units (`exp2ap`, `rngen`, `scales`, `asection`, `reverb`, `rankwave`, `addsynth`, `division`, `lfqueue`) and archives their object files as `libaeolus-core.a`. The artifact is uploaded as `aeolus-windows-x64-core`.
- This is intentionally a **core compilation probe**, not the Aeolus Windows application: it does not link the model/audio/threading/app layers, does not include a GUI, WASAPI, ASIO, or USB-MIDI, and does not produce `Aeolus.exe`. Its purpose is to obtain the first real MSYS2/Windows compiler feedback on portable synthesis code before attempting the full application.
- The workflow runs automatically on the next push to `windows-port` that changes source/windows/workflow files, and can also be started manually from GitHub Actions. Its result must be checked before calling the core Windows-compatible.


### 2026-11-12 — Expand the Windows compile probe

- Extended the Windows x64 CI compile set beyond the six low-level synthesis files to include `addsynth.cc` and `division.cc`, which cover organ stop definitions and division mixing.
- Replaced the legacy `__BYTE_ORDER` checks in `addsynth.cc` with the project's portable `AEOLUS_BYTE_ORDER` macros. This was necessary because the Windows portability macros introduced earlier were not yet used by this source file's binary read/write code.
- The CI artifact remains a static core archive, not the complete application. The application entry point, thread runtime, audio/MIDI backends, and GUI remain outstanding.


### 2026-11-12 — Compile the lock-free queue implementation

- Added `lfqueue.cc` to the Windows x64 CI compile probe. This covers the lock-free note/command queue storage used to hand events between the audio, model, and MIDI services, without requiring the external thread library.
- The probe still does not compile the worker classes: `Model`, `Slave`, `Audio`, and `Imidi` depend on `clthreads`, and `Imidi` still contains the ALSA-only implementation. Those are the next dependency boundaries to resolve.


### 2026-11-12 — Separate the ALSA MIDI translation unit

- Wrapped the existing ALSA sequencer implementation in `source/imidi.cc` with the Linux platform guard. This removes direct ALSA symbol references from the non-Linux compilation branch and prevents the Linux implementation from leaking into a future Windows build.
- Added a deliberately inert non-Linux `Imidi` thread stub so the translation unit has definitions while a native backend is not yet implemented. It does **not** enumerate devices, receive MIDI, or send MIDI. On Windows, USB-MIDI remains unavailable until the WinMM/MIDI Services implementation replaces this stub.
- Initialized the Linux sequencer handle to null in the constructor. This also makes the existing Linux shutdown path deterministic if the sequencer was never opened.
- This change is not yet validated by the Windows CI compile probe because `imidi.h` inherits from `A_thread` and the project still lacks a Windows build of `clthreads`. The next step is to establish that thread dependency, then include `imidi.cc` in the Windows compile target.


### 2026-11-12 — Vendor the clthreads source for a Windows build probe

- Imported the upstream `dmeliza/clthreads` source and its LGPL-2.1-or-later license into `third_party/clthreads/`, preserving copyright notices and documenting the exact upstream revision.
- Extended Windows CI to compile the six library translation units and the non-Linux `source/imidi.cc` translation unit using MSYS2 UCRT64 with POSIX thread flags. This is intended to test whether the chosen MinGW runtime can support the existing worker API without rewriting Aeolus's thread model.
- The imported dependency has not been modified. A successful compile would still not prove thread scheduling, cancellation, queue safety, or shutdown behaviour at runtime. The current Windows MIDI implementation remains a no-op stub.


### 2026-11-12 — First successful Windows compile of clthreads and MIDI boundary

- The MSYS2 UCRT64 Windows CI successfully compiled the vendored clthreads library sources and the non-Linux `source/imidi.cc` translation unit using the POSIX thread toolchain. This validates the dependency's basic compile path and the ALSA isolation guard; it does not validate runtime thread semantics or provide working Windows MIDI.
- Expanded the next CI pass to also compile `source/model.cc`, `source/slave.cc`, and `source/audio.cc` against the vendored thread library. This checks whether the shared model and audio host seams are now syntactically buildable on Windows before adding a native device backend.


### 2026-11-12 — Model/audio compile success and text-interface probe

- Windows CI run `43196199081` succeeded after adding the missing `VERSION=0.9.10` definition. The UCRT64 compiler now accepts `model.cc`, `slave.cc`, and `audio.cc` in addition to the previously tested synthesis units, vendored clthreads sources, and the isolated MIDI translation unit.
- This is still compile-only validation. It does not link or run Aeolus and does not mean the Windows audio or MIDI backends are operational.
- Extended the workflow to compile the existing readline-based `tiface.cc` text interface and added the matching UCRT64 readline package. The resulting run is pending; the X11/clxclient graphical interface remains unported and is not replaced by this text-interface check.
- The next substantial implementation task remains a real Windows audio host. The current `Audio::render_block()` renderer operates in 64-frame quanta; a WASAPI adapter must preserve that timeline when endpoint packets have different sizes, rather than padding or dropping samples. WASAPI's event-driven render path uses the Core Audio client/render-client interfaces and must be validated on a real Windows audio device.


### 2026-11-12 — Successful Windows compile coverage and link-test expansion

- GitHub Actions run `43198358610` passed on the UCRT64 Windows x64 runner for commit `b09c2ec`. The compile probe accepted the portable synthesis units, vendored `clthreads` sources, `imidi.cc`, `model.cc`, `slave.cc`, `audio.cc`, and `tiface.cc`.
- The follow-up run `43198798624` adds a small Windows executable smoke test that constructs and destroys the non-Linux `Imidi` object while linking the actual queue and clthreads objects. This is intended to catch missing constructor, destructor, vtable, and dependency symbols that a compile-only test cannot detect. The smoke test does not start a thread and does not test MIDI I/O.
- A successful smoke-test executable is only a link/lifecycle check for this narrow class boundary. It is not `Aeolus.exe`, and it does not validate the full application startup, audio rendering, MIDI devices, or GUI.
- The next major audio step remains a native WASAPI host. Before integrating it, retain the 64-frame processing constraint and add explicit buffering between arbitrary endpoint packet sizes and the engine's render quantum if endpoint negotiation cannot guarantee compatible packets.


### 2026-11-12 — Host audio packet/quantum adapter

- Added `windows/audio_quantum_fifo.h`, a small allocation-free adapter that serves arbitrary planar-float host packet sizes while invoking the existing Aeolus renderer only in complete 64-frame quanta.
- The adapter keeps any unused tail of a rendered quantum for the next device packet. This preserves sample continuity when WASAPI packet lengths are not multiples of 64; it does not pad callbacks or skip synthesis frames.
- Added a deterministic test using packet sizes 13, 79, 1, 127, 64, and 3 frames. It checks left/right sample continuity across callback boundaries and verifies that only full 64-frame blocks are rendered.
- Added the test to the Windows x64 GitHub Actions workflow. This validates the packet adaptation logic only; no WASAPI device client, COM initialization, endpoint enumeration, device format conversion, or real-time playback has been implemented yet.


- Follow-up: removed the old `init_host()` restriction that required the host's nominal packet size to be a multiple of 64. With the FIFO adapter in place, host packet sizes such as 480 frames are valid; only the internal renderer remains quantized to 64 frames.
- Expanded the FIFO test to include a 480-frame packet, representative of common audio-engine callback sizes. The test is part of the same Windows CI job.


### Host-render entry point wired into Audio (2026-11-12)

- On Windows, `Audio` now owns an `AudioQuantumFifo` configured by `init_host()`.
- New `render_host_packet(float **outputs, unsigned int nframes)` passes arbitrary-size planar-float packets through the FIFO and invokes the existing `render_block()` for complete 64-frame quanta.
- FIFO allocation and configuration happen during host initialization, not on the render callback. The render path itself performs no allocation or locking.
- This is the first connection from the host packet adapter into the actual Aeolus audio engine. WASAPI endpoint startup, COM/device lifetime, shared/exclusive format negotiation, interleaved-to-planar conversion, device-loss handling, and playback are still outstanding.


### 2026-11-12 — First native WASAPI render host

- Added `windows/wasapi_audio.h/.cc` with a native Windows Core Audio shared-mode render client.
- The host enumerates the default console render endpoint, negotiates the endpoint mix format, uses event-driven `IAudioClient` rendering, and obtains output packets through `IAudioRenderClient`.
- The callback receives preallocated planar float buffers; the host interleaves them into the endpoint buffer without allocating in the render loop. It accepts 32-bit IEEE-float mix formats with either two or four channels and reports the negotiated sample rate and buffer size to its caller.
- `stop()` signals the event so a blocking render loop can exit; COM/device setup and teardown are expected on the same thread that calls `open()` and `run()`.
- The new source is added to the Windows CI compile pass. This is a backend component, not yet connected to the application startup or an `.exe`.
- Current limitations are explicit: shared mode only; default endpoint only; float32 mix format only; stereo or four-channel output only; no endpoint hot-plug/recovery, device selector, exclusive mode, ASIO, or real-device playback validation. Unsupported mix formats fail cleanly rather than being misinterpreted.


### 2026-11-12 — Connect WASAPI to the Aeolus renderer

- Added `AeolusWasapiOutput` as the narrow host adapter between `WasapiAudioHost` and the existing `Audio::render_host_packet()` entry point.
- Opening the adapter now negotiates the Windows endpoint first, then configures the Aeolus engine with the endpoint's sample rate, channel count, and buffer size. The existing 64-frame FIFO remains responsible for packet/quantum mismatch.
- The application lifecycle remains intentionally explicit: after opening, the future Windows application must connect the model/audio message routes and call `Audio::start()`; it then calls the adapter's blocking `run()` on the same thread used for COM initialization. `stop()` may be signalled from another thread.
- Added a CI compile step for this adapter. It checks the source-level connection but does not instantiate an audio device or prove playback.
- Device selection, exclusive mode, sample format conversion for non-float endpoints, device-loss recovery, and the application entry point are still outstanding.


### 2026-11-12 — Tighten WASAPI format validation

- The WASAPI host now checks that extensible float formats advertise 32 valid bits, not merely a 32-bit container. It also validates the average byte rate against the negotiated sample rate and block alignment before treating the endpoint buffer as interleaved float samples.
- Removed an unused second interleaved scratch allocation; the host writes directly into the buffer returned by `IAudioRenderClient::GetBuffer`, while the planar render buffers remain preallocated.
- The adapter and host compile successfully in the preceding Windows CI run. The stricter validation/removal change is awaiting its own CI result. No physical Windows audio endpoint has been exercised yet.
