#ifndef EMBEDDED_SHADERS_INCLUDES_H
#define EMBEDDED_SHADERS_INCLUDES_H

// Fallback definitions for missing embedded shader files
#define SHADER_DATA_FALLBACK static const uint8_t shader_fallback_data[] = { 0 }; static const uint32_t shader_fallback_size = 1;

#if BGFX_PLATFORM_SUPPORTS_SPIRV
#if __has_include("shaders/generated/spirv/ui_vs.sc.bin.h")
#include "shaders/generated/spirv/ui_vs.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#if __has_include("shaders/generated/spirv/ui_ps.sc.bin.h")
#include "shaders/generated/spirv/ui_ps.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#if __has_include("shaders/generated/spirv/background_vs.sc.bin.h")
#include "shaders/generated/spirv/background_vs.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#if __has_include("shaders/generated/spirv/background_ps.sc.bin.h")
#include "shaders/generated/spirv/background_ps.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#if __has_include("shaders/generated/spirv/hdBackground_ps.sc.bin.h")
#include "shaders/generated/spirv/hdBackground_ps.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#if __has_include("shaders/generated/spirv/maskBackground_vs.sc.bin.h")
#include "shaders/generated/spirv/maskBackground_vs.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#if __has_include("shaders/generated/spirv/maskBackground_ps.sc.bin.h")
#include "shaders/generated/spirv/maskBackground_ps.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#if __has_include("shaders/generated/spirv/maskHDBackground_ps.sc.bin.h")
#include "shaders/generated/spirv/maskHDBackground_ps.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#if __has_include("shaders/generated/spirv/ramp_vs.sc.bin.h")
#include "shaders/generated/spirv/ramp_vs.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#if __has_include("shaders/generated/spirv/ramp_ps.sc.bin.h")
#include "shaders/generated/spirv/ramp_ps.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#if __has_include("shaders/generated/spirv/noise_vs.sc.bin.h")
#include "shaders/generated/spirv/noise_vs.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#if __has_include("shaders/generated/spirv/noise_ps.sc.bin.h")
#include "shaders/generated/spirv/noise_ps.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#if __has_include("shaders/generated/spirv/flat_vs.sc.bin.h")
#include "shaders/generated/spirv/flat_vs.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#if __has_include("shaders/generated/spirv/flat_ps.sc.bin.h")
#include "shaders/generated/spirv/flat_ps.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#if __has_include("shaders/generated/spirv/textured_ps.sc.bin.h")
#include "shaders/generated/spirv/textured_ps.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#if __has_include("shaders/generated/spirv/sphere_vs.sc.bin.h")
#include "shaders/generated/spirv/sphere_vs.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#if __has_include("shaders/generated/spirv/sphere_ps.sc.bin.h")
#include "shaders/generated/spirv/sphere_ps.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#if __has_include("shaders/generated/spirv/postprocess_vs.sc.bin.h")
#include "shaders/generated/spirv/postprocess_vs.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#if __has_include("shaders/generated/spirv/brightpass_ps.sc.bin.h")
#include "shaders/generated/spirv/brightpass_ps.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#if __has_include("shaders/generated/spirv/blur_ps.sc.bin.h")
#include "shaders/generated/spirv/blur_ps.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#if __has_include("shaders/generated/spirv/composite_ps.sc.bin.h")
#include "shaders/generated/spirv/composite_ps.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#if __has_include("shaders/generated/spirv/ssao_ps.sc.bin.h")
#include "shaders/generated/spirv/ssao_ps.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#if __has_include("shaders/generated/spirv/ssao_blur_ps.sc.bin.h")
#include "shaders/generated/spirv/ssao_blur_ps.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#if __has_include("shaders/generated/spirv/ssgi_ps.sc.bin.h")
#include "shaders/generated/spirv/ssgi_ps.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#if __has_include("shaders/generated/spirv/ssgi_blur_ps.sc.bin.h")
#include "shaders/generated/spirv/ssgi_blur_ps.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#if __has_include("shaders/generated/spirv/particle_vs.sc.bin.h")
#include "shaders/generated/spirv/particle_vs.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#if __has_include("shaders/generated/spirv/particle_ps.sc.bin.h")
#include "shaders/generated/spirv/particle_ps.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#if __has_include("shaders/generated/spirv/lantern_bloom_vs.sc.bin.h")
#include "shaders/generated/spirv/lantern_bloom_vs.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#if __has_include("shaders/generated/spirv/lantern_bloom_ps.sc.bin.h")
#include "shaders/generated/spirv/lantern_bloom_ps.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#if __has_include("shaders/generated/spirv/lantern_shadow_vs.sc.bin.h")
#include "shaders/generated/spirv/lantern_shadow_vs.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#if __has_include("shaders/generated/spirv/lantern_shadow_ps.sc.bin.h")
#include "shaders/generated/spirv/lantern_shadow_ps.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#endif

#if BGFX_PLATFORM_SUPPORTS_METAL
#if __has_include("shaders/generated/metal/ui_vs.sc.bin.h")
#include "shaders/generated/metal/ui_vs.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#if __has_include("shaders/generated/metal/ui_ps.sc.bin.h")
#include "shaders/generated/metal/ui_ps.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#if __has_include("shaders/generated/metal/background_vs.sc.bin.h")
#include "shaders/generated/metal/background_vs.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#if __has_include("shaders/generated/metal/background_ps.sc.bin.h")
#include "shaders/generated/metal/background_ps.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#if __has_include("shaders/generated/metal/hdBackground_ps.sc.bin.h")
#include "shaders/generated/metal/hdBackground_ps.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#if __has_include("shaders/generated/metal/maskBackground_vs.sc.bin.h")
#include "shaders/generated/metal/maskBackground_vs.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#if __has_include("shaders/generated/metal/maskBackground_ps.sc.bin.h")
#include "shaders/generated/metal/maskBackground_ps.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#if __has_include("shaders/generated/metal/maskHDBackground_ps.sc.bin.h")
#include "shaders/generated/metal/maskHDBackground_ps.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#if __has_include("shaders/generated/metal/ramp_vs.sc.bin.h")
#include "shaders/generated/metal/ramp_vs.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#if __has_include("shaders/generated/metal/ramp_ps.sc.bin.h")
#include "shaders/generated/metal/ramp_ps.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#if __has_include("shaders/generated/metal/noise_vs.sc.bin.h")
#include "shaders/generated/metal/noise_vs.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#if __has_include("shaders/generated/metal/noise_ps.sc.bin.h")
#include "shaders/generated/metal/noise_ps.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#if __has_include("shaders/generated/metal/flat_vs.sc.bin.h")
#include "shaders/generated/metal/flat_vs.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#if __has_include("shaders/generated/metal/flat_ps.sc.bin.h")
#include "shaders/generated/metal/flat_ps.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#if __has_include("shaders/generated/metal/textured_ps.sc.bin.h")
#include "shaders/generated/metal/textured_ps.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#if __has_include("shaders/generated/metal/sphere_vs.sc.bin.h")
#include "shaders/generated/metal/sphere_vs.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#if __has_include("shaders/generated/metal/sphere_ps.sc.bin.h")
#include "shaders/generated/metal/sphere_ps.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#if __has_include("shaders/generated/metal/postprocess_vs.sc.bin.h")
#include "shaders/generated/metal/postprocess_vs.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#if __has_include("shaders/generated/metal/brightpass_ps.sc.bin.h")
#include "shaders/generated/metal/brightpass_ps.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#if __has_include("shaders/generated/metal/blur_ps.sc.bin.h")
#include "shaders/generated/metal/blur_ps.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#if __has_include("shaders/generated/metal/composite_ps.sc.bin.h")
#include "shaders/generated/metal/composite_ps.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#if __has_include("shaders/generated/metal/ssao_ps.sc.bin.h")
#include "shaders/generated/metal/ssao_ps.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#if __has_include("shaders/generated/metal/ssao_blur_ps.sc.bin.h")
#include "shaders/generated/metal/ssao_blur_ps.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#if __has_include("shaders/generated/metal/ssgi_ps.sc.bin.h")
#include "shaders/generated/metal/ssgi_ps.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#if __has_include("shaders/generated/metal/ssgi_blur_ps.sc.bin.h")
#include "shaders/generated/metal/ssgi_blur_ps.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#if __has_include("shaders/generated/metal/particle_vs.sc.bin.h")
#include "shaders/generated/metal/particle_vs.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#if __has_include("shaders/generated/metal/particle_ps.sc.bin.h")
#include "shaders/generated/metal/particle_ps.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#if __has_include("shaders/generated/metal/lantern_bloom_vs.sc.bin.h")
#include "shaders/generated/metal/lantern_bloom_vs.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#if __has_include("shaders/generated/metal/lantern_bloom_ps.sc.bin.h")
#include "shaders/generated/metal/lantern_bloom_ps.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#if __has_include("shaders/generated/metal/lantern_shadow_vs.sc.bin.h")
#include "shaders/generated/metal/lantern_shadow_vs.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#if __has_include("shaders/generated/metal/lantern_shadow_ps.sc.bin.h")
#include "shaders/generated/metal/lantern_shadow_ps.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#endif

#if BGFX_PLATFORM_SUPPORTS_GLSL
#if __has_include("shaders/generated/glsl/ui_vs.sc.bin.h")
#include "shaders/generated/glsl/ui_vs.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#if __has_include("shaders/generated/glsl/ui_ps.sc.bin.h")
#include "shaders/generated/glsl/ui_ps.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#if __has_include("shaders/generated/glsl/background_vs.sc.bin.h")
#include "shaders/generated/glsl/background_vs.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#if __has_include("shaders/generated/glsl/background_ps.sc.bin.h")
#include "shaders/generated/glsl/background_ps.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#if __has_include("shaders/generated/glsl/hdBackground_ps.sc.bin.h")
#include "shaders/generated/glsl/hdBackground_ps.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#if __has_include("shaders/generated/glsl/maskBackground_vs.sc.bin.h")
#include "shaders/generated/glsl/maskBackground_vs.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#if __has_include("shaders/generated/glsl/maskBackground_ps.sc.bin.h")
#include "shaders/generated/glsl/maskBackground_ps.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#if __has_include("shaders/generated/glsl/maskHDBackground_ps.sc.bin.h")
#include "shaders/generated/glsl/maskHDBackground_ps.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#if __has_include("shaders/generated/glsl/ramp_vs.sc.bin.h")
#include "shaders/generated/glsl/ramp_vs.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#if __has_include("shaders/generated/glsl/ramp_ps.sc.bin.h")
#include "shaders/generated/glsl/ramp_ps.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#if __has_include("shaders/generated/glsl/noise_vs.sc.bin.h")
#include "shaders/generated/glsl/noise_vs.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#if __has_include("shaders/generated/glsl/noise_ps.sc.bin.h")
#include "shaders/generated/glsl/noise_ps.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#if __has_include("shaders/generated/glsl/flat_vs.sc.bin.h")
#include "shaders/generated/glsl/flat_vs.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#if __has_include("shaders/generated/glsl/flat_ps.sc.bin.h")
#include "shaders/generated/glsl/flat_ps.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#if __has_include("shaders/generated/glsl/textured_ps.sc.bin.h")
#include "shaders/generated/glsl/textured_ps.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#if __has_include("shaders/generated/glsl/sphere_vs.sc.bin.h")
#include "shaders/generated/glsl/sphere_vs.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#if __has_include("shaders/generated/glsl/sphere_ps.sc.bin.h")
#include "shaders/generated/glsl/sphere_ps.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#if __has_include("shaders/generated/glsl/postprocess_vs.sc.bin.h")
#include "shaders/generated/glsl/postprocess_vs.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#if __has_include("shaders/generated/glsl/brightpass_ps.sc.bin.h")
#include "shaders/generated/glsl/brightpass_ps.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#if __has_include("shaders/generated/glsl/blur_ps.sc.bin.h")
#include "shaders/generated/glsl/blur_ps.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#if __has_include("shaders/generated/glsl/composite_ps.sc.bin.h")
#include "shaders/generated/glsl/composite_ps.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#if __has_include("shaders/generated/glsl/ssao_ps.sc.bin.h")
#include "shaders/generated/glsl/ssao_ps.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#if __has_include("shaders/generated/glsl/ssao_blur_ps.sc.bin.h")
#include "shaders/generated/glsl/ssao_blur_ps.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#if __has_include("shaders/generated/glsl/ssgi_ps.sc.bin.h")
#include "shaders/generated/glsl/ssgi_ps.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#if __has_include("shaders/generated/glsl/ssgi_blur_ps.sc.bin.h")
#include "shaders/generated/glsl/ssgi_blur_ps.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#if __has_include("shaders/generated/glsl/particle_vs.sc.bin.h")
#include "shaders/generated/glsl/particle_vs.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#if __has_include("shaders/generated/glsl/particle_ps.sc.bin.h")
#include "shaders/generated/glsl/particle_ps.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#if __has_include("shaders/generated/glsl/lantern_bloom_vs.sc.bin.h")
#include "shaders/generated/glsl/lantern_bloom_vs.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#if __has_include("shaders/generated/glsl/lantern_bloom_ps.sc.bin.h")
#include "shaders/generated/glsl/lantern_bloom_ps.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#if __has_include("shaders/generated/glsl/lantern_shadow_vs.sc.bin.h")
#include "shaders/generated/glsl/lantern_shadow_vs.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#if __has_include("shaders/generated/glsl/lantern_shadow_ps.sc.bin.h")
#include "shaders/generated/glsl/lantern_shadow_ps.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#endif

#if BGFX_PLATFORM_SUPPORTS_DXBC
#if __has_include("shaders/generated/dx11/ui_vs.sc.bin.h")
#include "shaders/generated/dx11/ui_vs.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#if __has_include("shaders/generated/dx11/ui_ps.sc.bin.h")
#include "shaders/generated/dx11/ui_ps.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#if __has_include("shaders/generated/dx11/background_vs.sc.bin.h")
#include "shaders/generated/dx11/background_vs.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#if __has_include("shaders/generated/dx11/background_ps.sc.bin.h")
#include "shaders/generated/dx11/background_ps.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#if __has_include("shaders/generated/dx11/hdBackground_ps.sc.bin.h")
#include "shaders/generated/dx11/hdBackground_ps.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#if __has_include("shaders/generated/dx11/maskBackground_vs.sc.bin.h")
#include "shaders/generated/dx11/maskBackground_vs.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#if __has_include("shaders/generated/dx11/maskBackground_ps.sc.bin.h")
#include "shaders/generated/dx11/maskBackground_ps.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#if __has_include("shaders/generated/dx11/maskHDBackground_ps.sc.bin.h")
#include "shaders/generated/dx11/maskHDBackground_ps.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#if __has_include("shaders/generated/dx11/ramp_vs.sc.bin.h")
#include "shaders/generated/dx11/ramp_vs.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#if __has_include("shaders/generated/dx11/ramp_ps.sc.bin.h")
#include "shaders/generated/dx11/ramp_ps.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#if __has_include("shaders/generated/dx11/noise_vs.sc.bin.h")
#include "shaders/generated/dx11/noise_vs.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#if __has_include("shaders/generated/dx11/noise_ps.sc.bin.h")
#include "shaders/generated/dx11/noise_ps.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#if __has_include("shaders/generated/dx11/flat_vs.sc.bin.h")
#include "shaders/generated/dx11/flat_vs.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#if __has_include("shaders/generated/dx11/flat_ps.sc.bin.h")
#include "shaders/generated/dx11/flat_ps.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#if __has_include("shaders/generated/dx11/textured_ps.sc.bin.h")
#include "shaders/generated/dx11/textured_ps.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#if __has_include("shaders/generated/dx11/sphere_vs.sc.bin.h")
#include "shaders/generated/dx11/sphere_vs.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#if __has_include("shaders/generated/dx11/sphere_ps.sc.bin.h")
#include "shaders/generated/dx11/sphere_ps.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#if __has_include("shaders/generated/dx11/postprocess_vs.sc.bin.h")
#include "shaders/generated/dx11/postprocess_vs.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#if __has_include("shaders/generated/dx11/brightpass_ps.sc.bin.h")
#include "shaders/generated/dx11/brightpass_ps.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#if __has_include("shaders/generated/dx11/blur_ps.sc.bin.h")
#include "shaders/generated/dx11/blur_ps.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#if __has_include("shaders/generated/dx11/composite_ps.sc.bin.h")
#include "shaders/generated/dx11/composite_ps.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#if __has_include("shaders/generated/dx11/ssao_ps.sc.bin.h")
#include "shaders/generated/dx11/ssao_ps.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#if __has_include("shaders/generated/dx11/ssao_blur_ps.sc.bin.h")
#include "shaders/generated/dx11/ssao_blur_ps.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#if __has_include("shaders/generated/dx11/ssgi_ps.sc.bin.h")
#include "shaders/generated/dx11/ssgi_ps.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#if __has_include("shaders/generated/dx11/ssgi_blur_ps.sc.bin.h")
#include "shaders/generated/dx11/ssgi_blur_ps.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#if __has_include("shaders/generated/dx11/particle_vs.sc.bin.h")
#include "shaders/generated/dx11/particle_vs.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#if __has_include("shaders/generated/dx11/particle_ps.sc.bin.h")
#include "shaders/generated/dx11/particle_ps.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#if __has_include("shaders/generated/dx11/lantern_bloom_vs.sc.bin.h")
#include "shaders/generated/dx11/lantern_bloom_vs.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#if __has_include("shaders/generated/dx11/lantern_bloom_ps.sc.bin.h")
#include "shaders/generated/dx11/lantern_bloom_ps.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#if __has_include("shaders/generated/dx11/lantern_shadow_vs.sc.bin.h")
#include "shaders/generated/dx11/lantern_shadow_vs.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#if __has_include("shaders/generated/dx11/lantern_shadow_ps.sc.bin.h")
#include "shaders/generated/dx11/lantern_shadow_ps.sc.bin.h"
#else
SHADER_DATA_FALLBACK
#endif
#endif

#endif // EMBEDDED_SHADERS_INCLUDES_H
