///////////////////////////////////////////////////////////////////////////////
// Re-Haunted in-game remaster options dialog
///////////////////////////////////////////////////////////////////////////////
#pragma once

// Thread-safe because F1 is received on SDL's event thread while the dialog is
// rendered on the bgfx/game render thread.
bool remasterOptionsIsOpen();
void remasterOptionsToggle();
void remasterOptionsClose();
void remasterOptionsBeginStartupGate();
bool remasterOptionsIsStartupGate();

// Called inside the active ImGui frame, immediately before imguiEndFrame().
// Home and F1 toggle the dialog during normal play.
void remasterOptionsDraw();
