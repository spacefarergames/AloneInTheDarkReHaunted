///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Copyright (C) 2026 Infogrames / Spacefarer Retro Remasters LLC
// Based on FITD by yaz0r, Re-haunted is released under GPL
// Author: Jake Jackson (jake@spacefarergames.com)
//
// Post-processing effect declarations
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include <bgfx/bgfx.h>

// Post-processing effects manager
class PostProcessing {
public:
    PostProcessing();
    ~PostProcessing();

    void init(int width, int height);
    void shutdown();
    void resize(int width, int height);

    // Effect toggles
    void setBloomEnabled(bool enabled) { m_bloomEnabled = enabled; }
    void setFilmGrainEnabled(bool enabled) { m_filmGrainEnabled = enabled; }
    void setSSAOEnabled(bool enabled) { m_ssaoEnabled = enabled; }
    void setSSGIEnabled(bool enabled) { m_ssgiEnabled = enabled; }
    void setLightProbesEnabled(bool enabled) { m_lightProbesEnabled = enabled; }

    bool isBloomEnabled() const { return m_bloomEnabled; }
    bool isFilmGrainEnabled() const { return m_filmGrainEnabled; }
    bool isSSAOEnabled() const { return m_ssaoEnabled; }
    bool isSSGIEnabled() const { return m_ssgiEnabled; }
    bool isLightProbesEnabled() const { return m_lightProbesEnabled; }

    // Effect parameters
    void setBloomThreshold(float threshold) { m_bloomThreshold = threshold; }
    void setBloomIntensity(float intensity) { m_bloomIntensity = intensity; }
    void setFilmGrainIntensity(float intensity) { m_filmGrainIntensity = intensity; }
    void setSSAORadius(float radius) { m_ssaoRadius = radius; }
    void setSSAOIntensity(float intensity) { m_ssaoIntensity = intensity; }
    void setBloomPasses(int passes) { m_bloomPasses = (passes >= 1 && passes <= 4) ? passes : 2; }
    void setSSGIRadius(float radius) { m_ssgiRadius = radius; }
    void setSSGIIntensity(float intensity) { m_ssgiIntensity = intensity; }
    void setSSGINumSamples(int samples) { m_ssgiNumSamples = (samples >= 8 && samples <= 32) ? samples : 16; }
    void setLightProbeIntensity(float intensity) { m_lightProbeIntensity = intensity; }

    // Begin scene - render to main framebuffer
    void beginScene();
    
    // Apply all enabled effects and present to screen
    void endScene();

    // Get the main render target for game rendering
    bgfx::FrameBufferHandle getMainRenderTarget() const { return m_mainFB; }
    bgfx::TextureHandle getMainColorTexture() const { return m_mainColorTex; }
    int getWidth() const { return m_width; }
    int getHeight() const { return m_height; }

private:
    void createFramebuffers(int width, int height);
    void destroyFramebuffers();
    void createShaders();
    void destroyShaders();

    // Effect render functions
    void renderBloom();
    void renderSSAO();
    void renderSSGI();
    void renderFilmGrain();
    void renderFinalComposite();

    // Render targets
    bgfx::FrameBufferHandle m_mainFB = BGFX_INVALID_HANDLE;          // Main scene (color + depth)
    bgfx::FrameBufferHandle m_brightPassFB = BGFX_INVALID_HANDLE;    // Bloom bright pass
    bgfx::FrameBufferHandle m_blurH_FB = BGFX_INVALID_HANDLE;        // Horizontal blur
    bgfx::FrameBufferHandle m_blurV_FB = BGFX_INVALID_HANDLE;        // Vertical blur
    bgfx::FrameBufferHandle m_ssaoFB = BGFX_INVALID_HANDLE;          // Raw SSAO
    bgfx::FrameBufferHandle m_ssaoBlurFB = BGFX_INVALID_HANDLE;      // Blurred SSAO
    bgfx::FrameBufferHandle m_ssgiFB = BGFX_INVALID_HANDLE;          // Raw SSGI
    bgfx::FrameBufferHandle m_ssgiBlurFB = BGFX_INVALID_HANDLE;      // Blurred SSGI

    // Textures
    bgfx::TextureHandle m_mainColorTex = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle m_mainDepthTex = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle m_brightPassTex = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle m_blurH_Tex = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle m_blurV_Tex = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle m_ssaoTex = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle m_ssaoBlurTex = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle m_ssgiTex = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle m_ssgiBlurTex = BGFX_INVALID_HANDLE;

    // Shaders
    bgfx::ProgramHandle m_brightPassShader = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle m_blurShader = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle m_compositeShader = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle m_ssaoShader = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle m_ssaoBlurShader = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle m_ssgiShader = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle m_ssgiBlurShader = BGFX_INVALID_HANDLE;

    // Uniforms
    bgfx::UniformHandle u_bloomParams = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_filmGrainParams = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_blurDirection = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_time = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_ssaoParams = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_depthParams = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_ssgiParams = BGFX_INVALID_HANDLE;

    // Samplers
    bgfx::UniformHandle s_texColor = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_texBloom = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_texDepth = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_texAO = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_texSSGI = BGFX_INVALID_HANDLE;

    // Parameters
    bool m_bloomEnabled = false;
    bool m_filmGrainEnabled = false;
    bool m_ssaoEnabled = false;
    bool m_ssgiEnabled = false;
    bool m_lightProbesEnabled = false;

    float m_bloomThreshold = 0.6f;
    float m_bloomIntensity = 0.5f;
    float m_filmGrainIntensity = 0.05f;
    float m_ssaoRadius = 500.0f;
    float m_ssaoIntensity = 1.0f;
    int m_bloomPasses = 2;
    float m_ssgiRadius = 300.0f;
    float m_ssgiIntensity = 0.6f;
    int m_ssgiNumSamples = 16;
    float m_lightProbeIntensity = 0.5f;

    int m_width = 0;
    int m_height = 0;
    float m_time = 0.0f;
};

// Global instance
extern PostProcessing* g_postProcessing;
