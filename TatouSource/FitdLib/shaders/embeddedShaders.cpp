#include "embeddedShadersMacro.h"
#include "config.h"
#include "embeddedShadersIncludes.h"

static const bgfx::EmbeddedShader s_embeddedShaders[] =
{
    BGFX_EMBEDDED_SHADER(ui_vs),
    BGFX_EMBEDDED_SHADER(ui_ps),
    BGFX_EMBEDDED_SHADER(background_vs),
BGFX_EMBEDDED_SHADER(background_ps),
BGFX_EMBEDDED_SHADER(hdBackground_ps),
BGFX_EMBEDDED_SHADER(maskBackground_vs),
BGFX_EMBEDDED_SHADER(maskBackground_ps),
BGFX_EMBEDDED_SHADER(maskHDBackground_ps),
BGFX_EMBEDDED_SHADER(flat_vs),
BGFX_EMBEDDED_SHADER(flat_ps),
BGFX_EMBEDDED_SHADER(textured_ps),
BGFX_EMBEDDED_SHADER(noise_vs),
BGFX_EMBEDDED_SHADER(noise_ps),
BGFX_EMBEDDED_SHADER(ramp_vs),
BGFX_EMBEDDED_SHADER(ramp_ps),
    BGFX_EMBEDDED_SHADER(sphere_vs),
    BGFX_EMBEDDED_SHADER(sphere_ps),
BGFX_EMBEDDED_SHADER(postprocess_vs),
BGFX_EMBEDDED_SHADER(brightpass_ps),
BGFX_EMBEDDED_SHADER(blur_ps),
BGFX_EMBEDDED_SHADER(composite_ps),
BGFX_EMBEDDED_SHADER(ssao_ps),
BGFX_EMBEDDED_SHADER(ssao_blur_ps),
BGFX_EMBEDDED_SHADER(ssgi_ps),
BGFX_EMBEDDED_SHADER(ssgi_blur_ps),
BGFX_EMBEDDED_SHADER(particle_vs),
BGFX_EMBEDDED_SHADER(particle_ps),
BGFX_EMBEDDED_SHADER(lantern_bloom_vs),
BGFX_EMBEDDED_SHADER(lantern_bloom_ps),
BGFX_EMBEDDED_SHADER(lantern_shadow_vs),
BGFX_EMBEDDED_SHADER(lantern_shadow_ps),

BGFX_EMBEDDED_SHADER_END()
};

bgfx::ProgramHandle loadBgfxProgram(const std::string& VSFile, const std::string& PSFile)
{
bgfx::RendererType::Enum type = bgfx::getRendererType();

bgfx::ProgramHandle ProgramHandle = bgfx::createProgram(
bgfx::createEmbeddedShader(s_embeddedShaders, type, VSFile.c_str())
, bgfx::createEmbeddedShader(s_embeddedShaders, type, PSFile.c_str())
, true
);
assert(bgfx::isValid(ProgramHandle));
return ProgramHandle;
}