# Windows threading dependency investigation

## Why this is a blocking dependency

The application does not use threads only in one isolated module. The existing clthreads API is part of the engine's object model:

- Audio, Imidi, Model, Slave, and Iface derive from A_thread.
- The application starts and prioritises these workers with thr_start(...).
- Workers exchange events and heap-owned messages through send_event, put_event, get_event, get_event_timed, and ITC_ctrl::connect.
- The audio worker requests a real-time scheduling policy; the MIDI and model workers attempt real-time scheduling and fall back to normal scheduling.

Replacing only pthread_create-style calls would not be enough. The event/message transport, timed waits, shutdown/join behaviour, and scheduling interface must also work.

## Findings

- Aeolus currently depends on Fons Adriaensen's C++ clthreads library, not a generic C threads.h wrapper.
- A public mirror of the original clthreads source says it has moved to Codeberg. The mirror is archived, so it should not be treated as the authoritative upstream.
- The Windows threading API is different from POSIX. Microsoft documents native thread creation and waiting through Win32 APIs; C++ std::thread is also a supported modern alternative. Neither is a drop-in replacement for the A_thread and ITC event/message API.
- A separate cross-platform library such as PerformanC/CThreads supports Windows, but its API is not the Aeolus clthreads API. Adopting it would still require an adapter or a port of the Aeolus-facing classes.

## Recommended approach

1. Retrieve and inspect the current clthreads source from its maintained upstream before deciding whether to fork it.
2. Inventory the exact A_thread, event queue, message ownership, timed-wait, and priority APIs used by Aeolus.
3. Prefer a small Windows implementation that preserves the existing clthreads public API, if the upstream implementation is tightly coupled to POSIX. This minimises changes in the synthesiser/model code.
4. Map thread priorities deliberately. Do not assume POSIX SCHED_FIFO priorities have a direct Windows equivalent; use documented Windows scheduling facilities and test audio stability under CPU load.
5. Add deterministic tests for event delivery, timed waits, worker shutdown, and message ownership before switching the audio/MIDI backends.

## Not yet decided

- Whether to port clthreads itself or replace it with an internal compatibility layer.
- Whether the Windows build will use MSVC or MinGW as its primary compiler.
- Whether any upstream clthreads changes can be reused under its LGPL terms.

No third-party threading code has been copied into this repository. No Windows threading implementation or test has been committed yet.
