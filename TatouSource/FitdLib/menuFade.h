#pragma once
// ImGui-based fade overlay. Captures the currently rendered frame into a
// private texture and fades that capture from opaque to transparent over
// `durationSeconds`, smoothly revealing whatever is rendered underneath.
// Use at menu open and close for clean crossfade transitions.
void menuFadeStart(float durationSeconds = 0.35f);
void menuFadeCancel();
bool menuFadeIsActive();
// Call once per frame inside the ImGui frame brackets, just before imguiEndFrame().
void menuFadeUpdateAndRender();
// Release the capture texture (call on shutdown).
void menuFadeShutdown();
