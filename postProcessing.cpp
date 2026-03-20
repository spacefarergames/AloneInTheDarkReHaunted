///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Copyright (C) 2026 Infogrames / Spacefarer Retro Remasters LLC
// Author: Jake Jackson (jake@spacefarergames.com)
//
// Post-processing effects (bloom, film grain, SSAO)
///////////////////////////////////////////////////////////////////////////////

#include "postProcessing.h"
#include "configRemaster.h"
#include <bx/math.h>
#include <bgfx/embedded_shader.h>
#include <string>

extern bgfx::ProgramHandle loadBgfxProgram(const std::string& VSFile, const std::string& PSFile);

extern float nearVal;
extern float farVal;

PostProcessing* g_postProcessing = nullptr;

// Post-processing uses bgfx views 198-203 to avoid conflicts with the main game views
static const bgfx::ViewId kViewSSAO       = 198;
static const bgfx::ViewId kViewSSAOBlur   = 199;
static const bgfx::ViewId kViewBrightPass = 200;
static const bgfx::ViewId kViewBlurH     = 201;
static const bgfx::ViewId kViewBlurV     = 202;
static const bgfx::ViewId kViewComposite = 203;

PostProcessing::PostProcessing()
{
}

PostProcessing::~PostProcessing()
{
    shutdown();
}

void PostProcessing::init(int width, int height)
{
    m_width = width;
    m_height = height;

    // Create uniforms
    s_texColor = bgfx::createUniform("s_texColor", bgfx::UniformType::Sampler);
    s_texBloom = bgfx::createUniform("s_texBloom", bgfx::UniformType::Sampler);
    s_texDepth = bgfx::createUniform("s_texDepth", bgfx::UniformType::Sampler);
    s_texAO = bgfx::createUniform("s_texAO", bgfx::UniformType::Sampler);

    u_bloomParams = bgfx::createUniform("u_bloomParams", bgfx::UniformType::Vec4);
    u_filmGrainParams = bgfx::createUniform("u_filmGrainParams", bgfx::UniformType::Vec4);
    u_blurDirection = bgfx::createUniform("u_blurDirection", bgfx::UniformType::Vec4);
    u_time = bgfx::createUniform("u_time", bgfx::UniformType::Vec4);
    u_ssaoParams = bgfx::createUniform("u_ssaoParams", bgfx::UniformType::Vec4);
    u_depthParams = bgfx::createUniform("u_depthParams", bgfx::UniformType::Vec4);

    createFramebuffers(width, height);
    createShaders();
}

void PostProcessing::shutdown()
{
    destroyFramebuffers();
    destroyShaders();

    // Destroy uniforms
    if (bgfx::isValid(s_texColor)) bgfx::destroy(s_texColor);
    if (bgfx::isValid(s_texBloom)) bgfx::destroy(s_texBloom);
    if (bgfx::isValid(s_texDepth)) bgfx::destroy(s_texDepth);
    if (bgfx::isValid(s_texAO)) bgfx::destroy(s_texAO);
    if (bgfx::isValid(u_bloomParams)) bgfx::destroy(u_bloomParams);
    if (bgfx::isValid(u_filmGrainParams)) bgfx::destroy(u_filmGrainParams);
    if (bgfx::isValid(u_blurDirection)) bgfx::destroy(u_blurDirection);
    if (bgfx::isValid(u_time)) bgfx::destroy(u_time);
    if (bgfx::isValid(u_ssaoParams)) bgfx::destroy(u_ssaoParams);
    if (bgfx::isValid(u_depthParams)) bgfx::destroy(u_depthParams);

    s_texColor = BGFX_INVALID_HANDLE;
    s_texBloom = BGFX_INVALID_HANDLE;
    s_texDepth = BGFX_INVALID_HANDLE;
    s_texAO = BGFX_INVALID_HANDLE;
    u_bloomParams = BGFX_INVALID_HANDLE;
    u_filmGrainParams = BGFX_INVALID_HANDLE;
    u_blurDirection = BGFX_INVALID_HANDLE;
    u_time = BGFX_INVALID_HANDLE;
    u_ssaoParams = BGFX_INVALID_HANDLE;
    u_depthParams = BGFX_INVALID_HANDLE;
}

void PostProcessing::resize(int width, int height)
{
    if (m_width == width && m_height == height)
        return;

    m_width = width;
    m_height = height;

    destroyFramebuffers();
    createFramebuffers(width, height);
}

void PostProcessing::createFramebuffers(int width, int height)
{
    // Main render target - color + depth (depth readable for SSAO)
    m_mainColorTex = bgfx::createTexture2D(width, height, false, 1, bgfx::TextureFormat::RGBA8, 
        BGFX_TEXTURE_RT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP);
    m_mainDepthTex = bgfx::createTexture2D(width, height, false, 1, bgfx::TextureFormat::D24S8,
        BGFX_TEXTURE_RT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP);
    bgfx::TextureHandle mainAttach[] = { m_mainColorTex, m_mainDepthTex };
    m_mainFB = bgfx::createFrameBuffer(2, mainAttach, false);

    // SSAO render targets (full resolution for quality)
    m_ssaoTex = bgfx::createTexture2D(width, height, false, 1, bgfx::TextureFormat::R8,
        BGFX_TEXTURE_RT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP);
    bgfx::TextureHandle ssaoAttach[] = { m_ssaoTex };
    m_ssaoFB = bgfx::createFrameBuffer(1, ssaoAttach, false);

    m_ssaoBlurTex = bgfx::createTexture2D(width, height, false, 1, bgfx::TextureFormat::R8,
        BGFX_TEXTURE_RT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP);
    bgfx::TextureHandle ssaoBlurAttach[] = { m_ssaoBlurTex };
    m_ssaoBlurFB = bgfx::createFrameBuffer(1, ssaoBlurAttach, false);

    // Bloom bright pass (half resolution for performance)
    int bloomWidth = width / 2;
    int bloomHeight = height / 2;
    m_brightPassTex = bgfx::createTexture2D(bloomWidth, bloomHeight, false, 1, bgfx::TextureFormat::RGBA8,
        BGFX_TEXTURE_RT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP);
    bgfx::TextureHandle brightAttach[] = { m_brightPassTex };
    m_brightPassFB = bgfx::createFrameBuffer(1, brightAttach, false);

    // Blur passes (same resolution as bright pass)
    m_blurH_Tex = bgfx::createTexture2D(bloomWidth, bloomHeight, false, 1, bgfx::TextureFormat::RGBA8,
        BGFX_TEXTURE_RT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP);
    bgfx::TextureHandle blurHAttach[] = { m_blurH_Tex };
    m_blurH_FB = bgfx::createFrameBuffer(1, blurHAttach, false);

    m_blurV_Tex = bgfx::createTexture2D(bloomWidth, bloomHeight, false, 1, bgfx::TextureFormat::RGBA8,
        BGFX_TEXTURE_RT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP);
    bgfx::TextureHandle blurVAttach[] = { m_blurV_Tex };
    m_blurV_FB = bgfx::createFrameBuffer(1, blurVAttach, false);
}

void PostProcessing::destroyFramebuffers()
{
    // Destroy framebuffers first (they don't own textures since we passed false)
    if (bgfx::isValid(m_mainFB)) bgfx::destroy(m_mainFB);
    if (bgfx::isValid(m_brightPassFB)) bgfx::destroy(m_brightPassFB);
    if (bgfx::isValid(m_blurH_FB)) bgfx::destroy(m_blurH_FB);
    if (bgfx::isValid(m_blurV_FB)) bgfx::destroy(m_blurV_FB);
    if (bgfx::isValid(m_ssaoFB)) bgfx::destroy(m_ssaoFB);
    if (bgfx::isValid(m_ssaoBlurFB)) bgfx::destroy(m_ssaoBlurFB);

    // Destroy textures separately since FB doesn't own them
    if (bgfx::isValid(m_mainColorTex)) bgfx::destroy(m_mainColorTex);
    if (bgfx::isValid(m_mainDepthTex)) bgfx::destroy(m_mainDepthTex);
    if (bgfx::isValid(m_brightPassTex)) bgfx::destroy(m_brightPassTex);
    if (bgfx::isValid(m_blurH_Tex)) bgfx::destroy(m_blurH_Tex);
    if (bgfx::isValid(m_blurV_Tex)) bgfx::destroy(m_blurV_Tex);
    if (bgfx::isValid(m_ssaoTex)) bgfx::destroy(m_ssaoTex);
    if (bgfx::isValid(m_ssaoBlurTex)) bgfx::destroy(m_ssaoBlurTex);

    m_mainFB = BGFX_INVALID_HANDLE;
    m_brightPassFB = BGFX_INVALID_HANDLE;
    m_blurH_FB = BGFX_INVALID_HANDLE;
    m_blurV_FB = BGFX_INVALID_HANDLE;
    m_ssaoFB = BGFX_INVALID_HANDLE;
    m_ssaoBlurFB = BGFX_INVALID_HANDLE;

    m_mainColorTex = BGFX_INVALID_HANDLE;
    m_mainDepthTex = BGFX_INVALID_HANDLE;
    m_brightPassTex = BGFX_INVALID_HANDLE;
    m_blurH_Tex = BGFX_INVALID_HANDLE;
    m_blurV_Tex = BGFX_INVALID_HANDLE;
    m_ssaoTex = BGFX_INVALID_HANDLE;
    m_ssaoBlurTex = BGFX_INVALID_HANDLE;
}

void PostProcessing::createShaders()
{
    m_brightPassShader = loadBgfxProgram("postprocess_vs", "brightpass_ps");
    m_blurShader = loadBgfxProgram("postprocess_vs", "blur_ps");
    m_compositeShader = loadBgfxProgram("postprocess_vs", "composite_ps");
    m_ssaoShader = loadBgfxProgram("postprocess_vs", "ssao_ps");
    m_ssaoBlurShader = loadBgfxProgram("postprocess_vs", "ssao_blur_ps");
}

void PostProcessing::destroyShaders()
{
    if (bgfx::isValid(m_brightPassShader)) bgfx::destroy(m_brightPassShader);
    if (bgfx::isValid(m_blurShader)) bgfx::destroy(m_blurShader);
    if (bgfx::isValid(m_compositeShader)) bgfx::destroy(m_compositeShader);
    if (bgfx::isValid(m_ssaoShader)) bgfx::destroy(m_ssaoShader);
    if (bgfx::isValid(m_ssaoBlurShader)) bgfx::destroy(m_ssaoBlurShader);

    m_brightPassShader = BGFX_INVALID_HANDLE;
    m_blurShader = BGFX_INVALID_HANDLE;
    m_compositeShader = BGFX_INVALID_HANDLE;
    m_ssaoShader = BGFX_INVALID_HANDLE;
    m_ssaoBlurShader = BGFX_INVALID_HANDLE;
}

static void renderFullscreenQuad(bgfx::ViewId viewId, bgfx::ProgramHandle program)
{
    bgfx::VertexLayout layout;
    layout.begin()
        .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
        .end();

    bgfx::TransientVertexBuffer tvb;
    bgfx::allocTransientVertexBuffer(&tvb, 6, layout);

    struct Vertex {
        float pos[3];
        float uv[2];
    };

    Vertex* v = (Vertex*)tvb.data;

    // Triangle 1
    v[0] = { {-1.0f,  1.0f, 0.0f}, {0.0f, 0.0f} };
    v[1] = { { 1.0f, -1.0f, 0.0f}, {1.0f, 1.0f} };
    v[2] = { { 1.0f,  1.0f, 0.0f}, {1.0f, 0.0f} };

    // Triangle 2
    v[3] = { {-1.0f,  1.0f, 0.0f}, {0.0f, 0.0f} };
    v[4] = { {-1.0f, -1.0f, 0.0f}, {0.0f, 1.0f} };
    v[5] = { { 1.0f, -1.0f, 0.0f}, {1.0f, 1.0f} };

    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
    bgfx::setVertexBuffer(0, &tvb);
    bgfx::submit(viewId, program);
}

void PostProcessing::beginScene()
{
    // Set rendering to main offscreen buffer
    bgfx::setViewFrameBuffer(0, m_mainFB);
}

void PostProcessing::endScene()
{
    m_time += 0.016f;

    // If no effects enabled, just composite the scene to the backbuffer as-is
    if (!m_bloomEnabled && !m_filmGrainEnabled && !m_ssaoEnabled)
    {
        // Still need to composite from offscreen FB to backbuffer
        float bloomParams[4] = { m_bloomThreshold, 0.0f, 0.0f, 0.0f };
        float filmGrainParams[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        float ssaoParams[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        float timeParams[4] = { m_time, 0.0f, 0.0f, 0.0f };

        bgfx::setViewName(kViewComposite, "Composite");
        bgfx::setViewRect(kViewComposite, 0, 0, (uint16_t)m_width, (uint16_t)m_height);
        bgfx::setViewFrameBuffer(kViewComposite, BGFX_INVALID_HANDLE);
        bgfx::setViewClear(kViewComposite, BGFX_CLEAR_NONE);

        bgfx::setUniform(u_bloomParams, bloomParams);
        bgfx::setUniform(u_filmGrainParams, filmGrainParams);
        bgfx::setUniform(u_ssaoParams, ssaoParams);
        bgfx::setUniform(u_time, timeParams);
        bgfx::setTexture(0, s_texColor, m_mainColorTex);
        bgfx::setTexture(1, s_texBloom, m_mainColorTex); // dummy, won't contribute
        bgfx::setTexture(2, s_texAO, m_mainColorTex);    // dummy, won't contribute
        renderFullscreenQuad(kViewComposite, m_compositeShader);
        return;
    }

    // Apply SSAO
    if (m_ssaoEnabled)
    {
        renderSSAO();
    }

    // Apply bloom
    if (m_bloomEnabled)
    {
        renderBloom();
    }

    // Final composite
    renderFinalComposite();
}

void PostProcessing::renderBloom()
{
    int bloomWidth = m_width / 2;
    int bloomHeight = m_height / 2;

    // 1. Bright pass - extract bright pixels from main scene
    {
        float bloomParams[4] = { m_bloomThreshold, m_bloomIntensity, 0.0f, 0.0f };

        bgfx::setViewName(kViewBrightPass, "BrightPass");
        bgfx::setViewRect(kViewBrightPass, 0, 0, (uint16_t)bloomWidth, (uint16_t)bloomHeight);
        bgfx::setViewFrameBuffer(kViewBrightPass, m_brightPassFB);
        bgfx::setViewClear(kViewBrightPass, BGFX_CLEAR_COLOR, 0x000000ff);

        bgfx::setUniform(u_bloomParams, bloomParams);
        bgfx::setTexture(0, s_texColor, m_mainColorTex);
        renderFullscreenQuad(kViewBrightPass, m_brightPassShader);
    }

    // 2. Horizontal blur
    {
        float blurDir[4] = { 1.0f / (float)bloomWidth, 0.0f, 0.0f, 0.0f };

        bgfx::setViewName(kViewBlurH, "BlurH");
        bgfx::setViewRect(kViewBlurH, 0, 0, (uint16_t)bloomWidth, (uint16_t)bloomHeight);
        bgfx::setViewFrameBuffer(kViewBlurH, m_blurH_FB);
        bgfx::setViewClear(kViewBlurH, BGFX_CLEAR_COLOR, 0x000000ff);

        bgfx::setUniform(u_blurDirection, blurDir);
        bgfx::setTexture(0, s_texColor, m_brightPassTex);
        renderFullscreenQuad(kViewBlurH, m_blurShader);
    }

    // 3. Vertical blur
    {
        float blurDir[4] = { 0.0f, 1.0f / (float)bloomHeight, 0.0f, 0.0f };

        bgfx::setViewName(kViewBlurV, "BlurV");
        bgfx::setViewRect(kViewBlurV, 0, 0, (uint16_t)bloomWidth, (uint16_t)bloomHeight);
        bgfx::setViewFrameBuffer(kViewBlurV, m_blurV_FB);
        bgfx::setViewClear(kViewBlurV, BGFX_CLEAR_COLOR, 0x000000ff);

        bgfx::setUniform(u_blurDirection, blurDir);
        bgfx::setTexture(0, s_texColor, m_blurH_Tex);
        renderFullscreenQuad(kViewBlurV, m_blurShader);
    }
}

void PostProcessing::renderFilmGrain()
{
    // Applied during final composite
}

void PostProcessing::renderSSAO()
{
    float ssaoParams[4] = { m_ssaoRadius, 1.0f, m_ssaoIntensity, 0.0f };
    float depthParams[4] = { nearVal, farVal, 1.0f / (float)m_width, 1.0f / (float)m_height };
    float timeParams[4] = { m_time, 0.0f, 0.0f, 0.0f };

    // 1. SSAO pass - compute raw ambient occlusion from depth
    {
        bgfx::setViewName(kViewSSAO, "SSAO");
        bgfx::setViewRect(kViewSSAO, 0, 0, (uint16_t)m_width, (uint16_t)m_height);
        bgfx::setViewFrameBuffer(kViewSSAO, m_ssaoFB);
        bgfx::setViewClear(kViewSSAO, BGFX_CLEAR_COLOR, 0xffffffff);

        bgfx::setUniform(u_ssaoParams, ssaoParams);
        bgfx::setUniform(u_depthParams, depthParams);
        bgfx::setUniform(u_time, timeParams);
        bgfx::setTexture(0, s_texDepth, m_mainDepthTex);
        renderFullscreenQuad(kViewSSAO, m_ssaoShader);
    }

    // 2. Bilateral blur pass - smooth AO while preserving edges
    {
        float blurDir[4] = { 1.0f / (float)m_width, 1.0f / (float)m_height, 0.0f, 0.0f };

        bgfx::setViewName(kViewSSAOBlur, "SSAO Blur");
        bgfx::setViewRect(kViewSSAOBlur, 0, 0, (uint16_t)m_width, (uint16_t)m_height);
        bgfx::setViewFrameBuffer(kViewSSAOBlur, m_ssaoBlurFB);
        bgfx::setViewClear(kViewSSAOBlur, BGFX_CLEAR_COLOR, 0xffffffff);

        bgfx::setUniform(u_blurDirection, blurDir);
        bgfx::setUniform(u_depthParams, depthParams);
        bgfx::setTexture(0, s_texColor, m_ssaoTex);
        bgfx::setTexture(1, s_texDepth, m_mainDepthTex);
        renderFullscreenQuad(kViewSSAOBlur, m_ssaoBlurShader);
    }
}

void PostProcessing::renderFinalComposite()
{
    float bloomParams[4] = { m_bloomThreshold, m_bloomEnabled ? m_bloomIntensity : 0.0f, 0.0f, 0.0f };
    float filmGrainParams[4] = { m_filmGrainEnabled ? m_filmGrainIntensity : 0.0f, 0.0f, 0.0f, 0.0f };
    float ssaoParams[4] = { m_ssaoRadius, 1.0f, m_ssaoEnabled ? m_ssaoIntensity : 0.0f, 0.0f };
    float timeParams[4] = { m_time, 0.0f, 0.0f, 0.0f };

    bgfx::setViewName(kViewComposite, "Composite");
    bgfx::setViewRect(kViewComposite, 0, 0, (uint16_t)m_width, (uint16_t)m_height);
    bgfx::setViewFrameBuffer(kViewComposite, BGFX_INVALID_HANDLE); // render to backbuffer
    bgfx::setViewClear(kViewComposite, BGFX_CLEAR_NONE);

    bgfx::setUniform(u_bloomParams, bloomParams);
    bgfx::setUniform(u_filmGrainParams, filmGrainParams);
    bgfx::setUniform(u_ssaoParams, ssaoParams);
    bgfx::setUniform(u_time, timeParams);
    bgfx::setTexture(0, s_texColor, m_mainColorTex);
    bgfx::setTexture(1, s_texBloom, m_bloomEnabled ? m_blurV_Tex : m_mainColorTex);
    bgfx::setTexture(2, s_texAO, m_ssaoEnabled ? m_ssaoBlurTex : m_mainColorTex);
    renderFullscreenQuad(kViewComposite, m_compositeShader);
}
