#pragma once

#include <string>

// Starts/resumes automatic AITD2/AITD3 replacement-content generation.
// Source extraction is quick and synchronous; all Upscayl work is queued to
// a detached worker so gameplay and rendering remain responsive.
void hdAutoUpscale_Initialize();
void hdAutoUpscale_QueueFile(const std::string& sourcePath, const std::string& outputPath);
void hdAutoUpscale_QueueDirectory(const std::string& sourceDirectory, const std::string& outputDirectory);
void hdAutoUpscale_QueueInPlace(const std::string& path);
void hdAutoUpscale_DrawOverlay();
bool hdAutoUpscale_IsEnabled();

// Temporarily yields the external neural upscaler's GPU/CPU work to a
// render-critical section such as live movie playback. Queued work resumes
// automatically afterward.
void hdAutoUpscale_SetGameplayCritical(bool critical);
