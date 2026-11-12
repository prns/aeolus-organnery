# Native Windows organ-console UI

## Goal

Replace the X11/clxclient presentation with a native Win32 desktop interface while keeping Aeolus' existing model and message protocol. The Windows application must remain a standalone synthesizer, not a plugin or a generic MIDI monitor.

## What the Linux interface actually contains

The existing source/xiface.cc is the message/event bridge. Its windows are implemented in mainwin, editwin, audiowin, instrwin, and midiwin, with shared drawing, fonts, styles, callbacks, and widgets supplied by clxclient. midiwin.cc and audiowin.cc are X11 settings dialogs, not Windows backends. Porting only Xiface or renaming these files would not produce a working Windows UI.

## Proposed implementation

Use the Win32 window/message loop and GDI for the first native implementation. Keep the interface on its own UI thread and communicate with Model only by the existing ITC_ctrl messages. Do not call model methods directly from a window procedure.

1. **Window host and lifecycle**
   - Add a Wiface : Iface implementation with a top-level resizable window, DPI-aware client sizing, menu/command routing, and deterministic shutdown.
   - Run the Win32 message pump on the interface worker thread. Marshal model updates to that thread with posted private window messages.
   - Handle WM_CLOSE, WM_DESTROY, and EV_EXIT without blocking the audio thread or destroying controls from a model callback.
2. **Organ console**
   - Render the divisions and stop groups from M_ifc_init metadata.
   - Render stop state from MT_IFC_ELSET, MT_IFC_ELCLR, and MT_IFC_GRCLR updates.
   - Send M_ifc_ifelm messages for stop toggles and group actions.
   - Preserve the original stop labels, mnemonics, group ordering, keyboard mapping, and the 4:3, 16:9, and vertical arrangements.
3. **Settings and editing**
   - Port MIDI channel mapping, audio-section controls, instrument settings, and rank/temperament editing as separate dialogs after the main console interaction is stable.
   - Keep the original keyboard shortcuts and preset/save semantics.
4. **DPI, accessibility, and testing**
   - Use logical layout units and recompute geometry on WM_DPICHANGED.
   - Provide keyboard focus, accessible names, and high-contrast-safe state indicators; do not rely on colour alone.
   - Add model-message-to-view tests that do not require an interactive desktop, then add Windows UI automation on a hosted or self-hosted desktop runner.

## First vertical slice

The first usable slice should be the main organ console only: start the engine, show the instrument's divisions and stop groups, reflect model stop-state updates, and allow a stop to be toggled. MIDI/audio/settings dialogs follow. This avoids claiming that the entire X11 interface has been ported when only a window shell exists.

## Current status

This document records the implementation boundary and acceptance sequence. There is not yet a native Win32 GUI class in the application. The current Windows executable still uses Tiface (the text interface). The GUI milestone is therefore not complete.


## First native console slice (implemented)

The Windows executable now starts `Wiface` from `windows/wiface.cc` instead
of the console `Tiface`. This first native Win32/GDI window displays the
instrument divisions and their stop mnemonics, draws active stops in a
different state, and sends `MT_IFC_ELXOR` to the model when a stop is clicked.
Model updates `MT_IFC_ELSET`, `MT_IFC_ELCLR` and `MT_IFC_GRCLR` update the
window state on the interface thread.

This is deliberately a first vertical slice, not a replacement for the full
X11 interface: preset controls, transpose, tutti, settings dialogs, stop editing,
DPI-aware relayout, accessibility metadata and richer labels remain to be
ported. The window currently lays out up to eight divisions in two columns;
very large instruments may need scrolling in a later pass. Windows CI checks
compilation and linking; interactive behaviour still needs a Windows desktop
test.
