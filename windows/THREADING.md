# Windows threading dependency investigation

## Findings from the actual clthreads API

Aeolus does not call the Win32 or C++ threading API directly. Its worker classes depend on Fons Adriaensen's separate `clthreads` C++ library.

The public `clthreads.h` API (version 2.4.0-6 source mirror inspected for this investigation) shows that the library is built on POSIX threads:

- `P_thread` owns a `pthread_t`, exposes `thr_start(policy, priority, stacksize)`, and has a virtual `thr_main()`.
- `A_thread` combines `P_thread` with `ITC_ctrl`.
- `ITC_ctrl` provides event counters, message queues, `send_event()`, `get_event()`, `get_event_timed()`, and static `connect()`.
- The synchronisation layer uses pthread mutexes and condition variables. Linux additionally gets a semaphore wrapper.
- `thr_start()` and the underlying implementation also need review for POSIX scheduling and cancellation semantics; a pthread header alone is not proof that the library works correctly on Windows.

In Aeolus, `Audio`, `Imidi`, `Model`, `Slave`, and `Iface` inherit from `A_thread`. The application uses the event/message transport for engine control, not just for thread startup. Message ownership and shutdown ordering therefore need to remain compatible.

## Practical Windows build direction

For an initial Windows x64 port, MinGW-w64 with its POSIX-thread runtime is the least disruptive experiment:

1. Build a Windows-compatible `clthreads` library against the MinGW-w64 POSIX runtime.
2. Patch and test the POSIX scheduling calls in clthreads. Unsupported real-time policies must fail safely or map to a documented Windows priority class; they must not silently claim real-time scheduling.
3. Keep Aeolus's `A_thread` and `ITC_ctrl` call sites intact initially.
4. Add tests for event delivery, message ownership, timed waits, repeated start/stop, and worker shutdown.
5. Measure audio callback behaviour under CPU load before treating scheduling as acceptable.

This is a proposed route, not a verified build. The Windows pthread runtime does not by itself guarantee that the upstream clthreads implementation compiles or behaves correctly.

## MSVC versus MinGW

The current codebase is not ready for an MSVC build merely by changing compiler flags. A direct MSVC route would require either a supported pthread compatibility library plus fixes to clthreads, or a deliberate port of clthreads to Win32 primitives / C++ standard threading. That is a larger change because the condition-variable event protocol and message queues are part of the public API.

The project should choose one primary Windows compiler before adding build files. For the first proof of concept, MinGW-w64 POSIX is the smaller engineering step; MSVC compatibility can be assessed after the engine and device backends build.

## Licensing and provenance

No third-party threading implementation has been copied into this repository. The clthreads source inspected for API research carries an LGPL notice. Any vendoring or modification must preserve its applicable notices and be reviewed against the exact upstream version and license text.

## Remaining work

- Confirm the exact clthreads revision used by the target Linux distributions and obtain its authoritative maintained source.
- Build clthreads for x64 Windows and fix compile/runtime incompatibilities.
- Verify the Aeolus worker lifecycle and event protocol with automated tests.
- Only then connect the Windows audio and MIDI backends to the existing worker model.

No Windows threading implementation or successful Windows build has been committed yet.


## Vendored dependency and first Windows compile probe

The upstream clthreads source has now been imported into
`third_party/clthreads/` from `dmeliza/clthreads` revision
`7b59da40f3cae5a86eb23bc806c2d32e128680c5`. Its original LGPL-2.1-or-later
notices and COPYING file are retained. The imported source is not yet modified.

The Windows Actions workflow now attempts to compile all six clthreads source
units with the MSYS2 UCRT64 MinGW toolchain and POSIX thread support, then
compiles the isolated `source/imidi.cc` translation unit against that header.
This is the first concrete test of the proposed MinGW POSIX route. It does not
yet establish that the full Aeolus worker model links or runs correctly.
If the compiler rejects POSIX scheduling or timing APIs, those failures will
be addressed in the dependency rather than hidden behind a dummy thread layer.
