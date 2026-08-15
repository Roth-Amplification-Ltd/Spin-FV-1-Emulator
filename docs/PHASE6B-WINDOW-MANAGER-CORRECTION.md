# Phase 6B Rosie Window-Manager Correction

## Problem

Rosie exposed a native-window integration regression during Phase 6B acceptance: the maximize title-bar
control disappeared. The first attempted correction added minimize/maximize/close hints after
`QMainWindow` construction. That did not restore maximize and additionally broke normal minimize-to-dock
behavior.

## Root cause of the first correction

Changing QWidget window flags after construction is not an appropriate way to define the primary
application window contract. Qt may recreate/reparent a native window when its window flags are changed.
For a primary Linux desktop window that can alter how the window manager classifies and tracks the
window.

## Corrected implementation

`MainWindow` is now constructed with its complete desktop contract in the `QMainWindow` base
constructor:

- ordinary `Qt::Window` top-level type;
- title bar;
- system menu;
- minimize and maximize controls;
- close control.

No main-window flag mutation occurs after construction. The Qt smoke test also verifies normal top-level
identity and all three control hints.

## Acceptance

The final authority for title-bar buttons and dock/task-list integration is the real desktop compositor /
window manager. Rosie acceptance therefore requires both automated Qt smoke success and manual
confirmation that minimize, maximize, restore, and close behave normally.
