#pragma once
#include <cstdint>

#if defined(_WIN32)
#define FITD_UNITY_API extern "C" __declspec(dllexport)
#else
#define FITD_UNITY_API extern "C" __attribute__((visibility("default")))
#endif

// ABI v2 render packets: little-endian FUG2 magic, frame id, then records
// { uint16 opcode, uint32 payloadBytes, payload }. No managed callbacks are retained.
FITD_UNITY_API int FitdUnity_CopyGpuPacket(void* destination, int capacity);
FITD_UNITY_API void FitdUnity_CancelGpuWait();
FITD_UNITY_API void FitdUnity_AcknowledgeGpuPacket(uint32_t frame);
