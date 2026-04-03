///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Copyright (C) 2026 Infogrames / Spacefarer Retro Remasters LLC
// Based on FITD by yaz0r, Re-haunted is released under GPL
// Author: Jake Jackson (jake@spacefarergames.com)
//
// BGFX hardware-accelerated 3D rendering pipeline
///////////////////////////////////////////////////////////////////////////////

#include "common.h"
#include "consoleLog.h"
#include "configRemaster.h"

/***************************************************************************
mainSDL.cpp  -  description
-------------------
begin                : Mon Jun 3 2002
copyright            : (C) 2002 by Yaz0r
email                : yaz0r@yaz0r.net
***************************************************************************/

/***************************************************************************
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
***************************************************************************/

#include "osystem.h"
#include <bgfx/bgfx.h>
#include <bx/platform.h>
#include "shaders/embeddedShaders.h"
#include "imguiBGFX.h"
#include "hdBackgroundRenderer.h"
#include "hdBackground.h"
#include "embedded/embeddedData.h"
#include "debugger.h"
#include "configRemaster.h"
#include "input.h"
#include "postProcessing.h"
#include "resourceGC.h"
#include "modelAtlas.h"
#include <array>
#include <SDL.h>
#include <string>

// stb_image_write for mask dumping (static to avoid linker conflicts)
#define STB_IMAGE_WRITE_STATIC
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../ThirdParty/bgfx.cmake/bimg/3rdparty/stb/stb_image_write.h"

// stb_image for loading replacement mask PNGs (static to avoid linker conflicts)
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include "../ThirdParty/bgfx.cmake/bimg/3rdparty/stb/stb_image.h"

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

// Access to current model atlas from renderer
extern ModelAtlasData* getCurrentAtlas();

unsigned int gameViewId = 1;
bgfx::TextureHandle g_backgroundTexture = BGFX_INVALID_HANDLE;
bgfx::TextureHandle g_uiLayerTexture = BGFX_INVALID_HANDLE;
bgfx::TextureHandle g_paletteTexture = BGFX_INVALID_HANDLE;

// Frozen scene texture for menu backgrounds (sampleable GPU texture)
static bgfx::TextureHandle s_frozenSceneTex = BGFX_INVALID_HANDLE;
static int s_frozenSceneWidth = 0;
static int s_frozenSceneHeight = 0;

extern int outputResolution[2];

#define _USE_MATH_DEFINES
#include <math.h>

// globals
char backBuffer[512 * 256 * 3];

unsigned int ditherTexture = 0;

unsigned int    debugFontTexture = 0;

struct maskStruct
{
    bgfx::TextureHandle maskTexture = BGFX_INVALID_HANDLE;
    bgfx::VertexBufferHandle vertexBuffer = BGFX_INVALID_HANDLE;
    int maskX1 = 0;
    int maskY1 = 0;
    int maskX2 = 0;
    int maskY2 = 0;
};

std::vector<std::vector<maskStruct>> maskTextures; // [room][mask]

//vertex buffers for rendering
struct polyVertex
{
    float X;
    float Y;
    float Z;

    float U;
    float V;
};

struct sphereVertex
{
    float X;
    float Y;
    float Z;

    float U;
    float V;

    float centerX;
    float centerY;
    float size;
    float material;
};

#define NUM_MAX_FLAT_VERTICES 5000*3
#define NUM_MAX_NOISE_VERTICES 2000*3
#define NUM_MAX_TRANSPARENT_VERTICES 1000*2
#define NUM_MAX_RAMP_VERTICES 3000*3
#define NUM_MAX_SPHERES_VERTICES 3000*3
#define NUM_MAX_TEXTURED_VERTICES 5000*3

std::array<polyVertex, NUM_MAX_FLAT_VERTICES> flatVertices;
std::array<polyVertex, NUM_MAX_NOISE_VERTICES> noiseVertices;
std::array<polyVertex, NUM_MAX_TRANSPARENT_VERTICES> transparentVertices;
std::array<polyVertex, NUM_MAX_RAMP_VERTICES> rampVertices;
std::array<sphereVertex, NUM_MAX_SPHERES_VERTICES> sphereVertices;
std::vector<polyVertex> g_lineVertices;
std::array<polyVertex, NUM_MAX_TEXTURED_VERTICES> texturedVertices;

int numUsedFlatVertices = 0;
int numUsedNoiseVertices = 0;
int numUsedTransparentVertices = 0;
int numUsedRampVertices = 0;
int numUsedSpheres = 0;
int numSpheresPrimitives = 0;  // Counter for which sphere primitive we're rendering (for atlas grid indexing)
int numUsedTexturedVertices = 0;
bgfx::TextureHandle g_activeModelTexture = BGFX_INVALID_HANDLE;

//static unsigned long int zoom = 0;

float nearVal = 100;
float farVal = 100000;
float cameraZoom = 0;
float fov = 0;

char RGB_Pal[256 * 4];

unsigned int    backTexture;

int g_screenWidth = 0;
int g_screenHeight = 0;

void osystem_preinigGL()
{}

void osystem_initGL(int screenWidth, int screenHeight)
{
#if 0
#ifndef __APPLE__
    gl3wInit();
#endif

    g_screenWidth = screenWidth;
    g_screenHeight = screenHeight;

    //glEnable(GL_TEXTURE_2D);
    //glEnable(GL_CULL_FACE);

    checkGL();

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    checkGL();

    // glEnable(GL_DEPTH_TEST);
    //glDepthFunc(GL_LEQUAL);
    glDepthFunc(GL_LESS);

    glViewport(0, 0, g_screenWidth, g_screenHeight);
    checkGL();

    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);       // Black Background
    checkGL();

    // generate textures
    {
        int i;
        int j;

        unsigned char ditherMap[256 * 256 * 4];

        unsigned char* tempPtr = ditherMap;

        for (i = 0; i < 256; i++)
        {
            for (j = 0; j < 256; j++)
            {
                unsigned char ditherValue = rand() % 0x50;

                *(tempPtr++) = ditherValue;
                *(tempPtr++) = ditherValue;
                *(tempPtr++) = ditherValue;
                *(tempPtr++) = 255;
            }
        }

        //glBlendFunc(GL_SRC_ALPHA,GL_ONE);

        glGenTextures(1, &ditherTexture);
        glBindTexture(GL_TEXTURE_2D, ditherTexture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, ditherMap);
        checkGL();

        glBindTexture(GL_TEXTURE_2D, 0);
        checkGL();

        //glEnable(GL_TEXTURE_2D);
        //checkGL();
    }

    checkGL();

    osystem_initBuffer();

    checkGL();

    GLuint vao;

    glGenVertexArrays(1, &vao); checkGL();
    glBindVertexArray(vao); checkGL();
#endif
}

static bool s_darkRoomActive = false;
static const float DARK_ROOM_BRIGHTNESS = 0.10f;

static void uploadPaletteWithDarkening()
{
    if (s_darkRoomActive)
    {
        u8 darkPal[256 * 3];
        for (int i = 0; i < 256 * 3; i++)
        {
            darkPal[i] = (u8)(RGB_Pal[i] * DARK_ROOM_BRIGHTNESS);
        }
        bgfx::updateTexture2D(g_paletteTexture, 0, 0, 0, 0, 3, 256, bgfx::copy(darkPal, 256 * 3));
    }
    else
    {
        bgfx::updateTexture2D(g_paletteTexture, 0, 0, 0, 0, 3, 256, bgfx::copy(RGB_Pal, 256 * 3));
    }
}

void osystem_setPalette(u8* palette)
{
    memcpy(RGB_Pal, palette, 256 * 3);
    uploadPaletteWithDarkening();
}

void osystem_setPalette(palette_t* palette)
{
    for (int i = 0; i < 256; i++) {
        for (int j = 0; j < 3; j++) {
            RGB_Pal[i * 3 + j] = palette->at(i)[j];
        }
    }
    uploadPaletteWithDarkening();
}

void osystem_setDarkRoom(bool dark)
{
    s_darkRoomActive = dark;
    uploadPaletteWithDarkening();
}

struct s_vertexData
{
    float positions[3];
    float textures[2];
    float color[4];
};
s_vertexData gVertexArray[1024 * 1024];

bgfx::ShaderHandle loadBgfxShader(const std::string& filename)
{
    std::vector<u8> memBlob;
    FILE* fHandle = fopen(filename.c_str(), "rb");
    if (fHandle == nullptr)
        return BGFX_INVALID_HANDLE;
    fseek(fHandle, 0, SEEK_END);
    u32 size = ftell(fHandle);
    fseek(fHandle, 0, SEEK_SET);
    memBlob.resize(size);
    fread(&memBlob[0], 1, size, fHandle);
    fclose(fHandle);

    bgfx::ShaderHandle handle = bgfx::createShader(bgfx::copy(&memBlob[0], size));
    bgfx::setName(handle, filename.c_str());

    return handle;
}

bgfx::ProgramHandle getUIShader()
{
    static bgfx::ProgramHandle programHandle = BGFX_INVALID_HANDLE;
    if (!bgfx::isValid(programHandle))
    {
        programHandle = loadBgfxProgram("ui_vs", "ui_ps");
    }

    return programHandle;
}

bgfx::ProgramHandle getBackgroundShader()
{
    static bgfx::ProgramHandle programHandle = BGFX_INVALID_HANDLE;
    if (!bgfx::isValid(programHandle))
    {
        programHandle = loadBgfxProgram("background_vs", "background_ps");
    }

    return programHandle;
}

bgfx::ProgramHandle getHDBackgroundShader()
{
    static bgfx::ProgramHandle programHandle = BGFX_INVALID_HANDLE;
    if (!bgfx::isValid(programHandle))
    {
        programHandle = loadBgfxProgram("background_vs", "hdBackground_ps");
    }

    return programHandle;
}

bgfx::ProgramHandle getMaskBackgroundShader()
{
    static bgfx::ProgramHandle programHandle = BGFX_INVALID_HANDLE;
    if (!bgfx::isValid(programHandle))
    {
        programHandle = loadBgfxProgram("maskBackground_vs", "maskBackground_ps");
    }

    return programHandle;
}


bgfx::ProgramHandle getHDMaskBackgroundShader()
{
    static bgfx::ProgramHandle programHandle = BGFX_INVALID_HANDLE;
    if (!bgfx::isValid(programHandle))
    {
        programHandle = loadBgfxProgram("maskBackground_vs", "maskHDBackground_ps");
    }

    return programHandle;
}
bgfx::ProgramHandle getFlatShader()
{
    static bgfx::ProgramHandle programHandle = BGFX_INVALID_HANDLE;
    if (!bgfx::isValid(programHandle))
    {
        programHandle = loadBgfxProgram("flat_vs", "flat_ps");
    }

    return programHandle;
}

bgfx::ProgramHandle getNoiseShader()
{
    static bgfx::ProgramHandle programHandle = BGFX_INVALID_HANDLE;
    if (!bgfx::isValid(programHandle))
    {
        programHandle = loadBgfxProgram("noise_vs", "noise_ps");
    }

    return programHandle;
}

bgfx::ProgramHandle getRampShader()
{
    static bgfx::ProgramHandle programHandle = BGFX_INVALID_HANDLE;
    if (!bgfx::isValid(programHandle))
    {
        programHandle = loadBgfxProgram("ramp_vs", "ramp_ps");
    }

    return programHandle;
}

bgfx::ProgramHandle getSphereShader()
{
    static bgfx::ProgramHandle programHandle = BGFX_INVALID_HANDLE;
    if (!bgfx::isValid(programHandle))
    {
        programHandle = loadBgfxProgram("sphere_vs", "sphere_ps");
    }

    return programHandle;
}

bgfx::ProgramHandle getTexturedShader()
{
    static bgfx::ProgramHandle programHandle = BGFX_INVALID_HANDLE;
    if (!bgfx::isValid(programHandle))
    {
        programHandle = loadBgfxProgram("flat_vs", "textured_ps");
    }

    return programHandle;
}

void osystem_drawUILayer()
{
    bgfx::updateTexture2D(g_uiLayerTexture, 0, 0, 0, 0, 320, 200, bgfx::copy(uiLayer.data(), 320 * 200));

    if (backgroundMode == backgroundModeEnum_2D)
    {
        bgfx::VertexLayout layout;
        layout
            .begin()
            .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
            .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
            .end();

        bgfx::TransientVertexBuffer transientBuffer;
        bgfx::allocTransientVertexBuffer(&transientBuffer, 6, layout);

        struct sVertice
        {
            float position[3];
            float texcoord[2];
        };

        sVertice* pVertices = (sVertice*)transientBuffer.data;

        float quadVertices[6 * 3];
        float quadUV[6 * 2];

        // 0
        pVertices->position[0] = 0.f;
        pVertices->position[1] = 0.f;
        pVertices->position[2] = 1000.f;
        pVertices->texcoord[0] = 0.f;
        pVertices->texcoord[1] = 0.f;
        pVertices++;

        //2
        pVertices->position[0] = 320.f;
        pVertices->position[1] = 200.f;
        pVertices->position[2] = 1000.f;
        pVertices->texcoord[0] = 1.f;
        pVertices->texcoord[1] = 1.f;
        pVertices++;

        //1
        pVertices->position[0] = 320.f;
        pVertices->position[1] = 0.f;
        pVertices->position[2] = 1000.f;
        pVertices->texcoord[0] = 1.f;
        pVertices->texcoord[1] = 0.f;
        pVertices++;

        //------------------------
        //3
        pVertices->position[0] = 0.f;
        pVertices->position[1] = 0.f;
        pVertices->position[2] = 1000.f;
        pVertices->texcoord[0] = 0.f;
        pVertices->texcoord[1] = 0.f;
        pVertices++;

        //4
        pVertices->position[0] = 0.f;
        pVertices->position[1] = 200.f;
        pVertices->position[2] = 1000.f;
        pVertices->texcoord[0] = 0.f;
        pVertices->texcoord[1] = 1.f;
        pVertices++;

        //5
        pVertices->position[0] = 320.f;
        pVertices->position[1] = 200.f;
        pVertices->position[2] = 1000.f;
        pVertices->texcoord[0] = 1.f;
        pVertices->texcoord[1] = 1.f;
        pVertices++;

        static bgfx::UniformHandle backgroundTextureUniform = BGFX_INVALID_HANDLE;
        if (!bgfx::isValid(backgroundTextureUniform))
        {
            backgroundTextureUniform = bgfx::createUniform("s_backgroundTexture", bgfx::UniformType::Sampler);
        }
        static bgfx::UniformHandle paletteTextureUniform = BGFX_INVALID_HANDLE;
        if (!bgfx::isValid(paletteTextureUniform))
        {
            paletteTextureUniform = bgfx::createUniform("s_paletteTexture", bgfx::UniformType::Sampler);
        }


        bgfx::setState(0 | BGFX_STATE_WRITE_RGB
            | BGFX_STATE_MSAA
            | BGFX_STATE_DEPTH_TEST_ALWAYS
        );

        bgfx::setVertexBuffer(0, &transientBuffer);

        bgfx::setTexture(0, backgroundTextureUniform, g_uiLayerTexture);
        bgfx::setTexture(1, paletteTextureUniform, g_paletteTexture);
        bgfx::submit(gameViewId, getUIShader());
    }
}

// Portrait overlay textures for character selection screen
static bgfx::TextureHandle g_portraitTextures[2] = { BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE };
static bool g_portraitTexturesLoaded = false;

static void loadPortraitTextures()
{
    if (g_portraitTexturesLoaded)
        return;

    const char* filenames[2] = { "Carnby.png", "Emily.png" };

    for (int i = 0; i < 2; i++)
    {
        int width, height, channels;
        unsigned char* data = loadHDImageFile(filenames[i], &width, &height, &channels);
        if (!data)
            continue;

        // Convert to RGBA if needed
        unsigned char* rgbaData = data;
        bool needsFree = false;
        if (channels == 3)
        {
            rgbaData = (unsigned char*)malloc(width * height * 4);
            if (rgbaData)
            {
                for (int p = 0; p < width * height; p++)
                {
                    rgbaData[p * 4 + 0] = data[p * 3 + 0];
                    rgbaData[p * 4 + 1] = data[p * 3 + 1];
                    rgbaData[p * 4 + 2] = data[p * 3 + 2];
                    rgbaData[p * 4 + 3] = 255;
                }
                needsFree = true;
            }
        }

        if (rgbaData)
        {
            g_portraitTextures[i] = bgfx::createTexture2D(width, height, false, 1,
                bgfx::TextureFormat::RGBA8, BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP,
                bgfx::copy(rgbaData, width * height * 4));
        }

        if (needsFree)
            free(rgbaData);
        freeHDImageData(data);
    }

    g_portraitTexturesLoaded = true;
}

void osystem_drawPortraitOverlay(int choice)
{
    if (backgroundMode != backgroundModeEnum_2D)
        return;

    loadPortraitTextures();

    if (choice < 0 || choice > 1)
        return;
    if (!bgfx::isValid(g_portraitTextures[choice]))
        return;

    bgfx::VertexLayout layout;
    layout
        .begin()
        .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
        .end();

    bgfx::TransientVertexBuffer transientBuffer;
    bgfx::allocTransientVertexBuffer(&transientBuffer, 6, layout);

    struct sVertice
    {
        float position[3];
        float texcoord[2];
    };

    sVertice* pVertices = (sVertice*)transientBuffer.data;

    // Full-screen quad (0,0)-(320,200), Z=999 (in front of background at 1000)
    pVertices->position[0] = 0.f;   pVertices->position[1] = 0.f;   pVertices->position[2] = 999.f;
    pVertices->texcoord[0] = 0.f;   pVertices->texcoord[1] = 0.f;
    pVertices++;

    pVertices->position[0] = 320.f; pVertices->position[1] = 200.f; pVertices->position[2] = 999.f;
    pVertices->texcoord[0] = 1.f;   pVertices->texcoord[1] = 1.f;
    pVertices++;

    pVertices->position[0] = 320.f; pVertices->position[1] = 0.f;   pVertices->position[2] = 999.f;
    pVertices->texcoord[0] = 1.f;   pVertices->texcoord[1] = 0.f;
    pVertices++;

    pVertices->position[0] = 0.f;   pVertices->position[1] = 0.f;   pVertices->position[2] = 999.f;
    pVertices->texcoord[0] = 0.f;   pVertices->texcoord[1] = 0.f;
    pVertices++;

    pVertices->position[0] = 0.f;   pVertices->position[1] = 200.f; pVertices->position[2] = 999.f;
    pVertices->texcoord[0] = 0.f;   pVertices->texcoord[1] = 1.f;
    pVertices++;

    pVertices->position[0] = 320.f; pVertices->position[1] = 200.f; pVertices->position[2] = 999.f;
    pVertices->texcoord[0] = 1.f;   pVertices->texcoord[1] = 1.f;
    pVertices++;

    static bgfx::UniformHandle portraitTextureUniform = BGFX_INVALID_HANDLE;
    if (!bgfx::isValid(portraitTextureUniform))
    {
        portraitTextureUniform = bgfx::createUniform("s_backgroundTexture", bgfx::UniformType::Sampler);
    }
    static bgfx::UniformHandle fadeLevelUniform = BGFX_INVALID_HANDLE;
    if (!bgfx::isValid(fadeLevelUniform))
    {
        fadeLevelUniform = bgfx::createUniform("u_fadeLevel", bgfx::UniformType::Vec4);
    }

    bgfx::setState(0 | BGFX_STATE_WRITE_RGB
        | BGFX_STATE_WRITE_A
        | BGFX_STATE_MSAA
        | BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_INV_SRC_ALPHA)
    );

    bgfx::setVertexBuffer(0, &transientBuffer);
    bgfx::setTexture(0, portraitTextureUniform, g_portraitTextures[choice]);

    float fadeParams[4] = { g_fadeLevel, 0.0f, 0.0f, 0.0f };
    bgfx::setUniform(fadeLevelUniform, fadeParams);

    bgfx::submit(gameViewId, getHDBackgroundShader());
}

// Render the frozen scene snapshot into a specific screen rectangle (e.g. pause menu preview box).
// Coordinates are in game-space (320x200). Rendered at Z=999 (in front of background, behind overlays).
void osystem_drawFrozenScenePreview(float x1, float y1, float x2, float y2)
{
    if (backgroundMode != backgroundModeEnum_2D)
        return;

    if (!bgfx::isValid(s_frozenSceneTex))
        return;

    bgfx::VertexLayout layout;
    layout
        .begin()
        .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
        .end();

    bgfx::TransientVertexBuffer transientBuffer;
    bgfx::allocTransientVertexBuffer(&transientBuffer, 6, layout);

    struct sVertice
    {
        float position[3];
        float texcoord[2];
    };

    sVertice* pVertices = (sVertice*)transientBuffer.data;

    // Quad at Z=999 covering the preview rectangle
    pVertices->position[0] = x1;  pVertices->position[1] = y1;  pVertices->position[2] = 999.f;
    pVertices->texcoord[0] = 0.f; pVertices->texcoord[1] = 0.f;
    pVertices++;

    pVertices->position[0] = x2;  pVertices->position[1] = y2;  pVertices->position[2] = 999.f;
    pVertices->texcoord[0] = 1.f; pVertices->texcoord[1] = 1.f;
    pVertices++;

    pVertices->position[0] = x2;  pVertices->position[1] = y1;  pVertices->position[2] = 999.f;
    pVertices->texcoord[0] = 1.f; pVertices->texcoord[1] = 0.f;
    pVertices++;

    pVertices->position[0] = x1;  pVertices->position[1] = y1;  pVertices->position[2] = 999.f;
    pVertices->texcoord[0] = 0.f; pVertices->texcoord[1] = 0.f;
    pVertices++;

    pVertices->position[0] = x1;  pVertices->position[1] = y2;  pVertices->position[2] = 999.f;
    pVertices->texcoord[0] = 0.f; pVertices->texcoord[1] = 1.f;
    pVertices++;

    pVertices->position[0] = x2;  pVertices->position[1] = y2;  pVertices->position[2] = 999.f;
    pVertices->texcoord[0] = 1.f; pVertices->texcoord[1] = 1.f;
    pVertices++;

    static bgfx::UniformHandle previewTexUniform = BGFX_INVALID_HANDLE;
    if (!bgfx::isValid(previewTexUniform))
    {
        previewTexUniform = bgfx::createUniform("s_backgroundTexture", bgfx::UniformType::Sampler);
    }
    static bgfx::UniformHandle previewFadeUniform = BGFX_INVALID_HANDLE;
    if (!bgfx::isValid(previewFadeUniform))
    {
        previewFadeUniform = bgfx::createUniform("u_fadeLevel", bgfx::UniformType::Vec4);
    }

    bgfx::setState(0 | BGFX_STATE_WRITE_RGB
        | BGFX_STATE_MSAA
    );

    bgfx::setVertexBuffer(0, &transientBuffer);
    bgfx::setTexture(0, previewTexUniform, s_frozenSceneTex);

    float fadeParams[4] = { g_fadeLevel, 0.0f, 0.0f, 0.0f };
    bgfx::setUniform(previewFadeUniform, fadeParams);

    bgfx::submit(gameViewId, getHDBackgroundShader());
}

// System menu HD background overlay
static bgfx::TextureHandle g_systemMenuBGTexture = BGFX_INVALID_HANDLE;
static bool g_systemMenuBGLoaded = false;

static void loadSystemMenuTexture()
{
    if (g_systemMenuBGLoaded)
        return;

    int width, height, channels;
    unsigned char* data = loadHDImageFile("SystemMenu.png", &width, &height, &channels);
    if (!data)
    {
        g_systemMenuBGLoaded = true;
        return;
    }

    // Convert to RGBA if needed
    unsigned char* rgbaData = data;
    bool needsFree = false;
    if (channels == 3)
    {
        rgbaData = (unsigned char*)malloc(width * height * 4);
        if (rgbaData)
        {
            for (int p = 0; p < width * height; p++)
            {
                rgbaData[p * 4 + 0] = data[p * 3 + 0];
                rgbaData[p * 4 + 1] = data[p * 3 + 1];
                rgbaData[p * 4 + 2] = data[p * 3 + 2];
                rgbaData[p * 4 + 3] = 255;
            }
            needsFree = true;
        }
    }

    if (rgbaData)
    {
        g_systemMenuBGTexture = bgfx::createTexture2D(width, height, false, 1,
            bgfx::TextureFormat::RGBA8, BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP,
            bgfx::copy(rgbaData, width * height * 4));
    }

    if (needsFree)
        free(rgbaData);
    freeHDImageData(data);

    g_systemMenuBGLoaded = true;
}

void osystem_drawSystemMenuBackground()
{
    if (backgroundMode != backgroundModeEnum_2D)
        return;

    loadSystemMenuTexture();

    if (!bgfx::isValid(g_systemMenuBGTexture))
        return;

    bgfx::VertexLayout layout;
    layout
        .begin()
        .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
        .end();

    bgfx::TransientVertexBuffer transientBuffer;
    bgfx::allocTransientVertexBuffer(&transientBuffer, 6, layout);

    struct sVertice
    {
        float position[3];
        float texcoord[2];
    };

    sVertice* pVertices = (sVertice*)transientBuffer.data;

    // Full-screen quad (0,0)-(320,200), Z=999 (in front of background at 1000)
    pVertices->position[0] = 0.f;   pVertices->position[1] = 0.f;   pVertices->position[2] = 999.f;
    pVertices->texcoord[0] = 0.f;   pVertices->texcoord[1] = 0.f;
    pVertices++;

    pVertices->position[0] = 320.f; pVertices->position[1] = 200.f; pVertices->position[2] = 999.f;
    pVertices->texcoord[0] = 1.f;   pVertices->texcoord[1] = 1.f;
    pVertices++;

    pVertices->position[0] = 320.f; pVertices->position[1] = 0.f;   pVertices->position[2] = 999.f;
    pVertices->texcoord[0] = 1.f;   pVertices->texcoord[1] = 0.f;
    pVertices++;

    pVertices->position[0] = 0.f;   pVertices->position[1] = 0.f;   pVertices->position[2] = 999.f;
    pVertices->texcoord[0] = 0.f;   pVertices->texcoord[1] = 0.f;
    pVertices++;

    pVertices->position[0] = 0.f;   pVertices->position[1] = 200.f; pVertices->position[2] = 999.f;
    pVertices->texcoord[0] = 0.f;   pVertices->texcoord[1] = 1.f;
    pVertices++;

    pVertices->position[0] = 320.f; pVertices->position[1] = 200.f; pVertices->position[2] = 999.f;
    pVertices->texcoord[0] = 1.f;   pVertices->texcoord[1] = 1.f;
    pVertices++;

    static bgfx::UniformHandle sysMenuTexUniform = BGFX_INVALID_HANDLE;
    if (!bgfx::isValid(sysMenuTexUniform))
    {
        sysMenuTexUniform = bgfx::createUniform("s_backgroundTexture", bgfx::UniformType::Sampler);
    }
    static bgfx::UniformHandle sysMenuFadeUniform = BGFX_INVALID_HANDLE;
    if (!bgfx::isValid(sysMenuFadeUniform))
    {
        sysMenuFadeUniform = bgfx::createUniform("u_fadeLevel", bgfx::UniformType::Vec4);
    }

    bgfx::setState(0 | BGFX_STATE_WRITE_RGB
        | BGFX_STATE_WRITE_A
        | BGFX_STATE_MSAA
        | BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_INV_SRC_ALPHA)
    );

    bgfx::setVertexBuffer(0, &transientBuffer);
    bgfx::setTexture(0, sysMenuTexUniform, g_systemMenuBGTexture);

    float fadeParams[4] = { g_fadeLevel, 0.0f, 0.0f, 0.0f };
    bgfx::setUniform(sysMenuFadeUniform, fadeParams);

    bgfx::submit(gameViewId, getHDBackgroundShader());
}

// Save/Restore screen HD background overlay
static bgfx::TextureHandle g_saveRestoreBGTexture = BGFX_INVALID_HANDLE;
static bool g_saveRestoreBGLoaded = false;

static void loadSaveRestoreTexture()
{
    if (g_saveRestoreBGLoaded)
        return;

    int width, height, channels;
    unsigned char* data = loadHDImageFile("SaveRestoreScreen.png", &width, &height, &channels);
    if (!data)
    {
        g_saveRestoreBGLoaded = true;
        return;
    }

    unsigned char* rgbaData = data;
    bool needsFree = false;
    if (channels == 3)
    {
        rgbaData = (unsigned char*)malloc(width * height * 4);
        if (rgbaData)
        {
            for (int p = 0; p < width * height; p++)
            {
                rgbaData[p * 4 + 0] = data[p * 3 + 0];
                rgbaData[p * 4 + 1] = data[p * 3 + 1];
                rgbaData[p * 4 + 2] = data[p * 3 + 2];
                rgbaData[p * 4 + 3] = 255;
            }
            needsFree = true;
        }
    }

    if (rgbaData)
    {
        g_saveRestoreBGTexture = bgfx::createTexture2D(width, height, false, 1,
            bgfx::TextureFormat::RGBA8, BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP,
            bgfx::copy(rgbaData, width * height * 4));
    }

    if (needsFree)
        free(rgbaData);
    freeHDImageData(data);

    g_saveRestoreBGLoaded = true;
}

void osystem_drawSaveRestoreBackground()
{
    if (backgroundMode != backgroundModeEnum_2D)
        return;

    loadSaveRestoreTexture();

    if (!bgfx::isValid(g_saveRestoreBGTexture))
        return;

    bgfx::VertexLayout layout;
    layout
        .begin()
        .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
        .end();

    bgfx::TransientVertexBuffer transientBuffer;
    bgfx::allocTransientVertexBuffer(&transientBuffer, 6, layout);

    struct sVertice
    {
        float position[3];
        float texcoord[2];
    };

    sVertice* pVertices = (sVertice*)transientBuffer.data;

    pVertices->position[0] = 0.f;   pVertices->position[1] = 0.f;   pVertices->position[2] = 999.f;
    pVertices->texcoord[0] = 0.f;   pVertices->texcoord[1] = 0.f;
    pVertices++;

    pVertices->position[0] = 320.f; pVertices->position[1] = 200.f; pVertices->position[2] = 999.f;
    pVertices->texcoord[0] = 1.f;   pVertices->texcoord[1] = 1.f;
    pVertices++;

    pVertices->position[0] = 320.f; pVertices->position[1] = 0.f;   pVertices->position[2] = 999.f;
    pVertices->texcoord[0] = 1.f;   pVertices->texcoord[1] = 0.f;
    pVertices++;

    pVertices->position[0] = 0.f;   pVertices->position[1] = 0.f;   pVertices->position[2] = 999.f;
    pVertices->texcoord[0] = 0.f;   pVertices->texcoord[1] = 0.f;
    pVertices++;

    pVertices->position[0] = 0.f;   pVertices->position[1] = 200.f; pVertices->position[2] = 999.f;
    pVertices->texcoord[0] = 0.f;   pVertices->texcoord[1] = 1.f;
    pVertices++;

    pVertices->position[0] = 320.f; pVertices->position[1] = 200.f; pVertices->position[2] = 999.f;
    pVertices->texcoord[0] = 1.f;   pVertices->texcoord[1] = 1.f;
    pVertices++;

    static bgfx::UniformHandle saveRestoreTexUniform = BGFX_INVALID_HANDLE;
    if (!bgfx::isValid(saveRestoreTexUniform))
    {
        saveRestoreTexUniform = bgfx::createUniform("s_backgroundTexture", bgfx::UniformType::Sampler);
    }
    static bgfx::UniformHandle saveRestoreFadeUniform = BGFX_INVALID_HANDLE;
    if (!bgfx::isValid(saveRestoreFadeUniform))
    {
        saveRestoreFadeUniform = bgfx::createUniform("u_fadeLevel", bgfx::UniformType::Vec4);
    }

    bgfx::setState(0 | BGFX_STATE_WRITE_RGB
        | BGFX_STATE_WRITE_A
        | BGFX_STATE_MSAA
        | BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_INV_SRC_ALPHA)
    );

    bgfx::setVertexBuffer(0, &transientBuffer);
    bgfx::setTexture(0, saveRestoreTexUniform, g_saveRestoreBGTexture);

    float fadeParams[4] = { g_fadeLevel, 0.0f, 0.0f, 0.0f };
    bgfx::setUniform(saveRestoreFadeUniform, fadeParams);

    bgfx::submit(gameViewId, getHDBackgroundShader());
}

// Save slot preview HD texture - renders loaded PNG preview data as an HD quad
static bgfx::TextureHandle g_saveSlotPreviewTex = BGFX_INVALID_HANDLE;
static int g_saveSlotPreviewW = 0;
static int g_saveSlotPreviewH = 0;

void osystem_updateSaveSlotPreviewTexture(unsigned char* rgbaData, int width, int height)
{
    if (!rgbaData || width <= 0 || height <= 0)
    {
        if (bgfx::isValid(g_saveSlotPreviewTex))
        {
            bgfx::destroy(g_saveSlotPreviewTex);
            g_saveSlotPreviewTex = BGFX_INVALID_HANDLE;
        }
        g_saveSlotPreviewW = 0;
        g_saveSlotPreviewH = 0;
        return;
    }

    if (width != g_saveSlotPreviewW || height != g_saveSlotPreviewH)
    {
        if (bgfx::isValid(g_saveSlotPreviewTex))
            bgfx::destroy(g_saveSlotPreviewTex);

        g_saveSlotPreviewTex = bgfx::createTexture2D(width, height, false, 1,
            bgfx::TextureFormat::RGBA8, BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP,
            bgfx::copy(rgbaData, width * height * 4));
        g_saveSlotPreviewW = width;
        g_saveSlotPreviewH = height;
    }
    else
    {
        bgfx::updateTexture2D(g_saveSlotPreviewTex, 0, 0, 0, 0, width, height,
            bgfx::copy(rgbaData, width * height * 4));
    }
}

void osystem_drawSaveSlotPreviewHD(float x1, float y1, float x2, float y2)
{
    if (backgroundMode != backgroundModeEnum_2D)
        return;

    if (!bgfx::isValid(g_saveSlotPreviewTex))
        return;

    bgfx::VertexLayout layout;
    layout
        .begin()
        .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
        .end();

    bgfx::TransientVertexBuffer transientBuffer;
    bgfx::allocTransientVertexBuffer(&transientBuffer, 6, layout);

    struct sVertice
    {
        float position[3];
        float texcoord[2];
    };

    sVertice* pVertices = (sVertice*)transientBuffer.data;

    pVertices->position[0] = x1;  pVertices->position[1] = y1;  pVertices->position[2] = 998.f;
    pVertices->texcoord[0] = 0.f; pVertices->texcoord[1] = 0.f;
    pVertices++;

    pVertices->position[0] = x2;  pVertices->position[1] = y2;  pVertices->position[2] = 998.f;
    pVertices->texcoord[0] = 1.f; pVertices->texcoord[1] = 1.f;
    pVertices++;

    pVertices->position[0] = x2;  pVertices->position[1] = y1;  pVertices->position[2] = 998.f;
    pVertices->texcoord[0] = 1.f; pVertices->texcoord[1] = 0.f;
    pVertices++;

    pVertices->position[0] = x1;  pVertices->position[1] = y1;  pVertices->position[2] = 998.f;
    pVertices->texcoord[0] = 0.f; pVertices->texcoord[1] = 0.f;
    pVertices++;

    pVertices->position[0] = x1;  pVertices->position[1] = y2;  pVertices->position[2] = 998.f;
    pVertices->texcoord[0] = 0.f; pVertices->texcoord[1] = 1.f;
    pVertices++;

    pVertices->position[0] = x2;  pVertices->position[1] = y2;  pVertices->position[2] = 998.f;
    pVertices->texcoord[0] = 1.f; pVertices->texcoord[1] = 1.f;
    pVertices++;

    static bgfx::UniformHandle slotPreviewTexUniform = BGFX_INVALID_HANDLE;
    if (!bgfx::isValid(slotPreviewTexUniform))
    {
        slotPreviewTexUniform = bgfx::createUniform("s_backgroundTexture", bgfx::UniformType::Sampler);
    }
    static bgfx::UniformHandle slotPreviewFadeUniform = BGFX_INVALID_HANDLE;
    if (!bgfx::isValid(slotPreviewFadeUniform))
    {
        slotPreviewFadeUniform = bgfx::createUniform("u_fadeLevel", bgfx::UniformType::Vec4);
    }

    bgfx::setState(0 | BGFX_STATE_WRITE_RGB
        | BGFX_STATE_MSAA
    );

    bgfx::setVertexBuffer(0, &transientBuffer);
    bgfx::setTexture(0, slotPreviewTexUniform, g_saveSlotPreviewTex);

    float fadeParams[4] = { g_fadeLevel, 0.0f, 0.0f, 0.0f };
    bgfx::setUniform(slotPreviewFadeUniform, fadeParams);

    bgfx::submit(gameViewId, getHDBackgroundShader());
}

void osystem_destroySaveSlotPreviewTexture()
{
    if (bgfx::isValid(g_saveSlotPreviewTex))
    {
        bgfx::destroy(g_saveSlotPreviewTex);
        g_saveSlotPreviewTex = BGFX_INVALID_HANDLE;
    }
    g_saveSlotPreviewW = 0;
    g_saveSlotPreviewH = 0;
}

// Hotspot overlay - magnifying glass indicator for interactive objects
static bgfx::TextureHandle g_hotspotOverlayTexture = BGFX_INVALID_HANDLE;
static bool g_hotspotOverlayLoaded = false;

static void loadHotspotOverlayTexture()
{
    if (g_hotspotOverlayLoaded)
        return;

    int width, height, channels;
    unsigned char* data = loadHDImageFile("HotspotOverlay.png", &width, &height, &channels);
    if (!data)
    {
        g_hotspotOverlayLoaded = true;
        return;
    }

    unsigned char* rgbaData = data;
    bool needsFree = false;
    if (channels == 3)
    {
        rgbaData = (unsigned char*)malloc(width * height * 4);
        if (rgbaData)
        {
            for (int p = 0; p < width * height; p++)
            {
                rgbaData[p * 4 + 0] = data[p * 3 + 0];
                rgbaData[p * 4 + 1] = data[p * 3 + 1];
                rgbaData[p * 4 + 2] = data[p * 3 + 2];
                rgbaData[p * 4 + 3] = 255;
            }
            needsFree = true;
        }
    }

    if (rgbaData)
    {
        g_hotspotOverlayTexture = bgfx::createTexture2D(width, height, false, 1,
            bgfx::TextureFormat::RGBA8, BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP,
            bgfx::copy(rgbaData, width * height * 4));
    }

    if (needsFree)
        free(rgbaData);
    freeHDImageData(data);

    g_hotspotOverlayLoaded = true;
}

void osystem_drawHotspotOverlay(float opacity)
{
    if (backgroundMode != backgroundModeEnum_2D)
        return;

    loadHotspotOverlayTexture();

    if (!bgfx::isValid(g_hotspotOverlayTexture))
        return;

    if (opacity <= 0.0f)
        return;

    bgfx::VertexLayout layout;
    layout
        .begin()
        .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
        .end();

    bgfx::TransientVertexBuffer transientBuffer;
    bgfx::allocTransientVertexBuffer(&transientBuffer, 6, layout);

    struct sVertice
    {
        float position[3];
        float texcoord[2];
    };

    sVertice* pVertices = (sVertice*)transientBuffer.data;

    // Full-screen quad (0,0)-(320,200), Z=997 (in front of previews, behind SaveRestoreScreen)
    pVertices->position[0] = 0.f;   pVertices->position[1] = 0.f;   pVertices->position[2] = 997.f;
    pVertices->texcoord[0] = 0.f;   pVertices->texcoord[1] = 0.f;
    pVertices++;

    pVertices->position[0] = 320.f; pVertices->position[1] = 200.f; pVertices->position[2] = 997.f;
    pVertices->texcoord[0] = 1.f;   pVertices->texcoord[1] = 1.f;
    pVertices++;

    pVertices->position[0] = 320.f; pVertices->position[1] = 0.f;   pVertices->position[2] = 997.f;
    pVertices->texcoord[0] = 1.f;   pVertices->texcoord[1] = 0.f;
    pVertices++;

    pVertices->position[0] = 0.f;   pVertices->position[1] = 0.f;   pVertices->position[2] = 997.f;
    pVertices->texcoord[0] = 0.f;   pVertices->texcoord[1] = 0.f;
    pVertices++;

    pVertices->position[0] = 0.f;   pVertices->position[1] = 200.f; pVertices->position[2] = 997.f;
    pVertices->texcoord[0] = 0.f;   pVertices->texcoord[1] = 1.f;
    pVertices++;

    pVertices->position[0] = 320.f; pVertices->position[1] = 200.f; pVertices->position[2] = 997.f;
    pVertices->texcoord[0] = 1.f;   pVertices->texcoord[1] = 1.f;
    pVertices++;

    static bgfx::UniformHandle hotspotTexUniform = BGFX_INVALID_HANDLE;
    static bgfx::UniformHandle hotspotFadeUniform = BGFX_INVALID_HANDLE;
    if (!bgfx::isValid(hotspotTexUniform))
    {
        hotspotTexUniform = bgfx::createUniform("s_backgroundTexture", bgfx::UniformType::Sampler);
        hotspotFadeUniform = bgfx::createUniform("u_fadeLevel", bgfx::UniformType::Vec4);
    }

    bgfx::setState(0 | BGFX_STATE_WRITE_RGB
        | BGFX_STATE_WRITE_A
        | BGFX_STATE_MSAA
        | BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_INV_SRC_ALPHA)
    );

    bgfx::setVertexBuffer(0, &transientBuffer);
    bgfx::setTexture(0, hotspotTexUniform, g_hotspotOverlayTexture);

    float fadeParams[4] = { g_fadeLevel * opacity, 0.0f, 0.0f, 0.0f };
    bgfx::setUniform(hotspotFadeUniform, fadeParams);

    bgfx::submit(gameViewId, getHDBackgroundShader());
}

// Startup menu HD background overlay
#define STARTUP_MENU_MAX_ANIM_FRAMES 512
static bgfx::TextureHandle g_startupMenuBGTexture = BGFX_INVALID_HANDLE;
static bgfx::TextureHandle g_startupMenuBGFrames[STARTUP_MENU_MAX_ANIM_FRAMES];
static int g_startupMenuBGFrameCount = 0;
static float g_startupMenuBGFrameTime = 0.08f;
static bool g_startupMenuBGLoaded = false;

// Helper: load image data, convert to RGBA, and create a bgfx texture
static bgfx::TextureHandle createMenuTexture(unsigned char* data, int width, int height, int channels)
{
    bgfx::TextureHandle tex = BGFX_INVALID_HANDLE;
    unsigned char* rgbaData = data;
    bool needsFree = false;
    if (channels == 3)
    {
        rgbaData = (unsigned char*)malloc(width * height * 4);
        if (rgbaData)
        {
            for (int p = 0; p < width * height; p++)
            {
                rgbaData[p * 4 + 0] = data[p * 3 + 0];
                rgbaData[p * 4 + 1] = data[p * 3 + 1];
                rgbaData[p * 4 + 2] = data[p * 3 + 2];
                rgbaData[p * 4 + 3] = 255;
            }
            needsFree = true;
        }
    }
    if (rgbaData)
    {
        tex = bgfx::createTexture2D(width, height, false, 1,
            bgfx::TextureFormat::RGBA8, BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP,
            bgfx::copy(rgbaData, width * height * 4));
    }
    if (needsFree)
        free(rgbaData);
    return tex;
}

static void loadStartupMenuTexture()
{
    if (g_startupMenuBGLoaded)
        return;

    // Try loading animation frames: StartupMenuBackground_001.png, _002.png, ...
    int baseWidth = 0, baseHeight = 0;
    for (int i = 1; i <= STARTUP_MENU_MAX_ANIM_FRAMES; i++)
    {
        char frameName[64];
        if (g_remasterConfig.graphics.enableArtwork)
            snprintf(frameName, sizeof(frameName), "StartupMenuBackgroundWithArt_%03d.png", i);
        else
            snprintf(frameName, sizeof(frameName), "StartupMenuBackground_%03d.png", i);

        int width, height, channels;
        unsigned char* data = loadHDImageFile(frameName, &width, &height, &channels);
        if (!data)
            break;

        if (g_startupMenuBGFrameCount == 0)
        {
            baseWidth = width;
            baseHeight = height;
        }
        else if (width != baseWidth || height != baseHeight)
        {
            freeHDImageData(data);
            break;
        }

        bgfx::TextureHandle tex = createMenuTexture(data, width, height, channels);
        freeHDImageData(data);

        if (!bgfx::isValid(tex))
            break;

        g_startupMenuBGFrames[g_startupMenuBGFrameCount++] = tex;
    }

    if (g_startupMenuBGFrameCount > 0)
    {
        g_startupMenuBGTexture = g_startupMenuBGFrames[0];
        g_startupMenuBGLoaded = true;
        return;
    }

    // Fall back to single static image
    int width, height, channels;
    const char* bgFileName = g_remasterConfig.graphics.enableArtwork
        ? "StartupMenuBackgroundWithArt.png"
        : "StartupMenuBackground.png";
    unsigned char* data = loadHDImageFile(bgFileName, &width, &height, &channels);
    if (!data)
    {
        g_startupMenuBGLoaded = true;
        return;
    }

    g_startupMenuBGTexture = createMenuTexture(data, width, height, channels);
    freeHDImageData(data);

    g_startupMenuBGLoaded = true;
}

void osystem_drawStartupMenuBackground()
{
    if (backgroundMode != backgroundModeEnum_2D)
        return;

    loadStartupMenuTexture();

    // Select animation frame based on elapsed time, or use static texture
    bgfx::TextureHandle activeTex = g_startupMenuBGTexture;
    if (g_startupMenuBGFrameCount > 1)
    {
        u32 now = (u32)SDL_GetTicks();
        int frameIdx = (int)((float)now / (g_startupMenuBGFrameTime * 1000.0f)) % g_startupMenuBGFrameCount;
        activeTex = g_startupMenuBGFrames[frameIdx];
    }

    if (!bgfx::isValid(activeTex))
        return;

    bgfx::VertexLayout layout;
    layout
        .begin()
        .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
        .end();

    bgfx::TransientVertexBuffer transientBuffer;
    bgfx::allocTransientVertexBuffer(&transientBuffer, 6, layout);

    struct sVertice
    {
        float position[3];
        float texcoord[2];
    };

    sVertice* pVertices = (sVertice*)transientBuffer.data;

    // Full-screen quad (0,0)-(320,200), Z=999 (in front of background at 1000)
    pVertices->position[0] = 0.f;   pVertices->position[1] = 0.f;   pVertices->position[2] = 999.f;
    pVertices->texcoord[0] = 0.f;   pVertices->texcoord[1] = 0.f;
    pVertices++;

    pVertices->position[0] = 320.f; pVertices->position[1] = 200.f; pVertices->position[2] = 999.f;
    pVertices->texcoord[0] = 1.f;   pVertices->texcoord[1] = 1.f;
    pVertices++;

    pVertices->position[0] = 320.f; pVertices->position[1] = 0.f;   pVertices->position[2] = 999.f;
    pVertices->texcoord[0] = 1.f;   pVertices->texcoord[1] = 0.f;
    pVertices++;

    pVertices->position[0] = 0.f;   pVertices->position[1] = 0.f;   pVertices->position[2] = 999.f;
    pVertices->texcoord[0] = 0.f;   pVertices->texcoord[1] = 0.f;
    pVertices++;

    pVertices->position[0] = 0.f;   pVertices->position[1] = 200.f; pVertices->position[2] = 999.f;
    pVertices->texcoord[0] = 0.f;   pVertices->texcoord[1] = 1.f;
    pVertices++;

    pVertices->position[0] = 320.f; pVertices->position[1] = 200.f; pVertices->position[2] = 999.f;
    pVertices->texcoord[0] = 1.f;   pVertices->texcoord[1] = 1.f;
    pVertices++;

    static bgfx::UniformHandle startupMenuTexUniform = BGFX_INVALID_HANDLE;
    if (!bgfx::isValid(startupMenuTexUniform))
    {
        startupMenuTexUniform = bgfx::createUniform("s_backgroundTexture", bgfx::UniformType::Sampler);
    }
    static bgfx::UniformHandle startupMenuFadeUniform = BGFX_INVALID_HANDLE;
    if (!bgfx::isValid(startupMenuFadeUniform))
    {
        startupMenuFadeUniform = bgfx::createUniform("u_fadeLevel", bgfx::UniformType::Vec4);
    }

    bgfx::setState(0 | BGFX_STATE_WRITE_RGB
        | BGFX_STATE_WRITE_A
        | BGFX_STATE_MSAA
        | BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_INV_SRC_ALPHA)
    );

    bgfx::setVertexBuffer(0, &transientBuffer);
    bgfx::setTexture(0, startupMenuTexUniform, activeTex);

    float fadeParams[4] = { g_fadeLevel, 0.0f, 0.0f, 0.0f };
    bgfx::setUniform(startupMenuFadeUniform, fadeParams);

    bgfx::submit(gameViewId, getHDBackgroundShader());
}

// Language selection HD background overlay
static bgfx::TextureHandle g_langSelectBGTexture = BGFX_INVALID_HANDLE;
static bool g_langSelectBGLoaded = false;

static void loadLanguageSelectionTexture()
{
    if (g_langSelectBGLoaded)
        return;

    int width, height, channels;
    unsigned char* data = loadHDImageFile("LanguageSelection.png", &width, &height, &channels);
    if (!data)
    {
        g_langSelectBGLoaded = true;
        return;
    }

    // Convert to RGBA if needed
    unsigned char* rgbaData = data;
    bool needsFree = false;
    if (channels == 3)
    {
        rgbaData = (unsigned char*)malloc(width * height * 4);
        if (rgbaData)
        {
            for (int p = 0; p < width * height; p++)
            {
                rgbaData[p * 4 + 0] = data[p * 3 + 0];
                rgbaData[p * 4 + 1] = data[p * 3 + 1];
                rgbaData[p * 4 + 2] = data[p * 3 + 2];
                rgbaData[p * 4 + 3] = 255;
            }
            needsFree = true;
        }
    }

    if (rgbaData)
    {
        g_langSelectBGTexture = bgfx::createTexture2D(width, height, false, 1,
            bgfx::TextureFormat::RGBA8, BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP,
            bgfx::copy(rgbaData, width * height * 4));
    }

    if (needsFree)
        free(rgbaData);
    freeHDImageData(data);

    g_langSelectBGLoaded = true;
}

void osystem_drawLanguageSelectionBackground()
{
    if (backgroundMode != backgroundModeEnum_2D)
        return;

    loadLanguageSelectionTexture();

    if (!bgfx::isValid(g_langSelectBGTexture))
        return;

    bgfx::VertexLayout layout;
    layout
        .begin()
        .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
        .end();

    bgfx::TransientVertexBuffer transientBuffer;
    bgfx::allocTransientVertexBuffer(&transientBuffer, 6, layout);

    struct sVertice
    {
        float position[3];
        float texcoord[2];
    };

    sVertice* pVertices = (sVertice*)transientBuffer.data;

    // Full-screen quad (0,0)-(320,200), Z=999 (in front of background at 1000)
    pVertices->position[0] = 0.f;   pVertices->position[1] = 0.f;   pVertices->position[2] = 999.f;
    pVertices->texcoord[0] = 0.f;   pVertices->texcoord[1] = 0.f;
    pVertices++;

    pVertices->position[0] = 320.f; pVertices->position[1] = 200.f; pVertices->position[2] = 999.f;
    pVertices->texcoord[0] = 1.f;   pVertices->texcoord[1] = 1.f;
    pVertices++;

    pVertices->position[0] = 320.f; pVertices->position[1] = 0.f;   pVertices->position[2] = 999.f;
    pVertices->texcoord[0] = 1.f;   pVertices->texcoord[1] = 0.f;
    pVertices++;

    pVertices->position[0] = 0.f;   pVertices->position[1] = 0.f;   pVertices->position[2] = 999.f;
    pVertices->texcoord[0] = 0.f;   pVertices->texcoord[1] = 0.f;
    pVertices++;

    pVertices->position[0] = 0.f;   pVertices->position[1] = 200.f; pVertices->position[2] = 999.f;
    pVertices->texcoord[0] = 0.f;   pVertices->texcoord[1] = 1.f;
    pVertices++;

    pVertices->position[0] = 320.f; pVertices->position[1] = 200.f; pVertices->position[2] = 999.f;
    pVertices->texcoord[0] = 1.f;   pVertices->texcoord[1] = 1.f;
    pVertices++;

    static bgfx::UniformHandle langSelectTexUniform = BGFX_INVALID_HANDLE;
    if (!bgfx::isValid(langSelectTexUniform))
    {
        langSelectTexUniform = bgfx::createUniform("s_backgroundTexture", bgfx::UniformType::Sampler);
    }
    static bgfx::UniformHandle langSelectFadeUniform = BGFX_INVALID_HANDLE;
    if (!bgfx::isValid(langSelectFadeUniform))
    {
        langSelectFadeUniform = bgfx::createUniform("u_fadeLevel", bgfx::UniformType::Vec4);
    }

    bgfx::setState(0 | BGFX_STATE_WRITE_RGB
        | BGFX_STATE_WRITE_A
        | BGFX_STATE_MSAA
        | BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_INV_SRC_ALPHA)
    );

    bgfx::setVertexBuffer(0, &transientBuffer);
    bgfx::setTexture(0, langSelectTexUniform, g_langSelectBGTexture);

    float fadeParams[4] = { g_fadeLevel, 0.0f, 0.0f, 0.0f };
    bgfx::setUniform(langSelectFadeUniform, fadeParams);

    bgfx::submit(gameViewId, getHDBackgroundShader());
}

// Full-screen frame HD overlay (used by controls menu, etc.)
static bgfx::TextureHandle g_fullScreenFrameTexture = BGFX_INVALID_HANDLE;
static bool g_fullScreenFrameLoaded = false;

static void loadFullScreenFrameTexture()
{
    if (g_fullScreenFrameLoaded)
        return;

    int width, height, channels;
    unsigned char* data = loadHDImageFile("FullScreenFrame.png", &width, &height, &channels);
    if (!data)
    {
        g_fullScreenFrameLoaded = true;
        return;
    }

    unsigned char* rgbaData = data;
    bool needsFree = false;
    if (channels == 3)
    {
        rgbaData = (unsigned char*)malloc(width * height * 4);
        if (rgbaData)
        {
            for (int p = 0; p < width * height; p++)
            {
                rgbaData[p * 4 + 0] = data[p * 3 + 0];
                rgbaData[p * 4 + 1] = data[p * 3 + 1];
                rgbaData[p * 4 + 2] = data[p * 3 + 2];
                rgbaData[p * 4 + 3] = 255;
            }
            needsFree = true;
        }
    }

    if (rgbaData)
    {
        g_fullScreenFrameTexture = bgfx::createTexture2D(width, height, false, 1,
            bgfx::TextureFormat::RGBA8, BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP,
            bgfx::copy(rgbaData, width * height * 4));
    }

    if (needsFree)
        free(rgbaData);
    freeHDImageData(data);

    g_fullScreenFrameLoaded = true;
}

void osystem_drawFullScreenFrame()
{
    if (backgroundMode != backgroundModeEnum_2D)
        return;

    loadFullScreenFrameTexture();

    if (!bgfx::isValid(g_fullScreenFrameTexture))
        return;

    bgfx::VertexLayout layout;
    layout
        .begin()
        .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
        .end();

    bgfx::TransientVertexBuffer transientBuffer;
    bgfx::allocTransientVertexBuffer(&transientBuffer, 6, layout);

    struct sVertice
    {
        float position[3];
        float texcoord[2];
    };

    sVertice* pVertices = (sVertice*)transientBuffer.data;

    pVertices->position[0] = 0.f;   pVertices->position[1] = 0.f;   pVertices->position[2] = 999.f;
    pVertices->texcoord[0] = 0.f;   pVertices->texcoord[1] = 0.f;
    pVertices++;

    pVertices->position[0] = 320.f; pVertices->position[1] = 200.f; pVertices->position[2] = 999.f;
    pVertices->texcoord[0] = 1.f;   pVertices->texcoord[1] = 1.f;
    pVertices++;

    pVertices->position[0] = 320.f; pVertices->position[1] = 0.f;   pVertices->position[2] = 999.f;
    pVertices->texcoord[0] = 1.f;   pVertices->texcoord[1] = 0.f;
    pVertices++;

    pVertices->position[0] = 0.f;   pVertices->position[1] = 0.f;   pVertices->position[2] = 999.f;
    pVertices->texcoord[0] = 0.f;   pVertices->texcoord[1] = 0.f;
    pVertices++;

    pVertices->position[0] = 0.f;   pVertices->position[1] = 200.f; pVertices->position[2] = 999.f;
    pVertices->texcoord[0] = 0.f;   pVertices->texcoord[1] = 1.f;
    pVertices++;

    pVertices->position[0] = 320.f; pVertices->position[1] = 200.f; pVertices->position[2] = 999.f;
    pVertices->texcoord[0] = 1.f;   pVertices->texcoord[1] = 1.f;
    pVertices++;

    static bgfx::UniformHandle fsFrameTexUniform = BGFX_INVALID_HANDLE;
    if (!bgfx::isValid(fsFrameTexUniform))
    {
        fsFrameTexUniform = bgfx::createUniform("s_backgroundTexture", bgfx::UniformType::Sampler);
    }
    static bgfx::UniformHandle fsFrameFadeUniform = BGFX_INVALID_HANDLE;
    if (!bgfx::isValid(fsFrameFadeUniform))
    {
        fsFrameFadeUniform = bgfx::createUniform("u_fadeLevel", bgfx::UniformType::Vec4);
    }

    bgfx::setState(0 | BGFX_STATE_WRITE_RGB
        | BGFX_STATE_WRITE_A
        | BGFX_STATE_MSAA
        | BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_INV_SRC_ALPHA)
    );

    bgfx::setVertexBuffer(0, &transientBuffer);
    bgfx::setTexture(0, fsFrameTexUniform, g_fullScreenFrameTexture);

    float fadeParams[4] = { g_fadeLevel, 0.0f, 0.0f, 0.0f };
    bgfx::setUniform(fsFrameFadeUniform, fadeParams);

    bgfx::submit(gameViewId, getHDBackgroundShader());
}

// Inventory HD background overlay
static bgfx::TextureHandle g_inventoryBGTexture = BGFX_INVALID_HANDLE;
static bool g_inventoryBGLoaded = false;

static void loadInventoryBGTexture()
{
    if (g_inventoryBGLoaded)
        return;

    int width, height, channels;
    unsigned char* data = loadHDImageFile("INVENTORYBG.png", &width, &height, &channels);
    if (!data)
    {
        g_inventoryBGLoaded = true;
        return;
    }

    // Convert to RGBA if needed
    unsigned char* rgbaData = data;
    bool needsFree = false;
    if (channels == 3)
    {
        rgbaData = (unsigned char*)malloc(width * height * 4);
        if (rgbaData)
        {
            for (int p = 0; p < width * height; p++)
            {
                rgbaData[p * 4 + 0] = data[p * 3 + 0];
                rgbaData[p * 4 + 1] = data[p * 3 + 1];
                rgbaData[p * 4 + 2] = data[p * 3 + 2];
                rgbaData[p * 4 + 3] = 255;
            }
            needsFree = true;
        }
    }

    if (rgbaData)
    {
        g_inventoryBGTexture = bgfx::createTexture2D(width, height, false, 1,
            bgfx::TextureFormat::RGBA8, BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP,
            bgfx::copy(rgbaData, width * height * 4));
    }

    if (needsFree)
        free(rgbaData);
    freeHDImageData(data);

    g_inventoryBGLoaded = true;
}

void osystem_drawInventoryBackground()
{
    if (backgroundMode != backgroundModeEnum_2D)
        return;

    loadInventoryBGTexture();

    if (!bgfx::isValid(g_inventoryBGTexture))
        return;

    bgfx::VertexLayout layout;
    layout
        .begin()
        .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
        .end();

    bgfx::TransientVertexBuffer transientBuffer;
    bgfx::allocTransientVertexBuffer(&transientBuffer, 6, layout);

    struct sVertice
    {
        float position[3];
        float texcoord[2];
    };

    sVertice* pVertices = (sVertice*)transientBuffer.data;

    // Full-screen quad (0,0)-(320,200), Z=999 (in front of background at 1000)
    pVertices->position[0] = 0.f;   pVertices->position[1] = 0.f;   pVertices->position[2] = 999.f;
    pVertices->texcoord[0] = 0.f;   pVertices->texcoord[1] = 0.f;
    pVertices++;

    pVertices->position[0] = 320.f; pVertices->position[1] = 200.f; pVertices->position[2] = 999.f;
    pVertices->texcoord[0] = 1.f;   pVertices->texcoord[1] = 1.f;
    pVertices++;

    pVertices->position[0] = 320.f; pVertices->position[1] = 0.f;   pVertices->position[2] = 999.f;
    pVertices->texcoord[0] = 1.f;   pVertices->texcoord[1] = 0.f;
    pVertices++;

    pVertices->position[0] = 0.f;   pVertices->position[1] = 0.f;   pVertices->position[2] = 999.f;
    pVertices->texcoord[0] = 0.f;   pVertices->texcoord[1] = 0.f;
    pVertices++;

    pVertices->position[0] = 0.f;   pVertices->position[1] = 200.f; pVertices->position[2] = 999.f;
    pVertices->texcoord[0] = 0.f;   pVertices->texcoord[1] = 1.f;
    pVertices++;

    pVertices->position[0] = 320.f; pVertices->position[1] = 200.f; pVertices->position[2] = 999.f;
    pVertices->texcoord[0] = 1.f;   pVertices->texcoord[1] = 1.f;
    pVertices++;

    static bgfx::UniformHandle invBGTextureUniform = BGFX_INVALID_HANDLE;
    if (!bgfx::isValid(invBGTextureUniform))
    {
        invBGTextureUniform = bgfx::createUniform("s_backgroundTexture", bgfx::UniformType::Sampler);
    }
    static bgfx::UniformHandle invFadeLevelUniform = BGFX_INVALID_HANDLE;
    if (!bgfx::isValid(invFadeLevelUniform))
    {
        invFadeLevelUniform = bgfx::createUniform("u_fadeLevel", bgfx::UniformType::Vec4);
    }

    bgfx::setState(0 | BGFX_STATE_WRITE_RGB
        | BGFX_STATE_WRITE_A
        | BGFX_STATE_MSAA
        | BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_INV_SRC_ALPHA)
    );

    bgfx::setVertexBuffer(0, &transientBuffer);
    bgfx::setTexture(0, invBGTextureUniform, g_inventoryBGTexture);

    float fadeParams[4] = { g_fadeLevel, 0.0f, 0.0f, 0.0f };
    bgfx::setUniform(invFadeLevelUniform, fadeParams);

    bgfx::submit(gameViewId, getHDBackgroundShader());
}

// Found object HD background overlay
static bgfx::TextureHandle g_foundObjectBGTexture = BGFX_INVALID_HANDLE;
static bool g_foundObjectBGLoaded = false;

static void loadFoundObjectBGTexture()
{
    if (g_foundObjectBGLoaded)
        return;

    int width, height, channels;
    unsigned char* data = loadHDImageFile("FoundObject.png", &width, &height, &channels);
    if (!data)
    {
        g_foundObjectBGLoaded = true;
        return;
    }

    // Convert to RGBA if needed
    unsigned char* rgbaData = data;
    bool needsFree = false;
    if (channels == 3)
    {
        rgbaData = (unsigned char*)malloc(width * height * 4);
        if (rgbaData)
        {
            for (int p = 0; p < width * height; p++)
            {
                rgbaData[p * 4 + 0] = data[p * 3 + 0];
                rgbaData[p * 4 + 1] = data[p * 3 + 1];
                rgbaData[p * 4 + 2] = data[p * 3 + 2];
                rgbaData[p * 4 + 3] = 255;
            }
            needsFree = true;
        }
    }

    if (rgbaData)
    {
        g_foundObjectBGTexture = bgfx::createTexture2D(width, height, false, 1,
            bgfx::TextureFormat::RGBA8, BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP,
            bgfx::copy(rgbaData, width * height * 4));
    }

    if (needsFree)
        free(rgbaData);
    freeHDImageData(data);

    g_foundObjectBGLoaded = true;
}

void osystem_drawFoundObjectBackground()
{
    if (backgroundMode != backgroundModeEnum_2D)
        return;

    loadFoundObjectBGTexture();

    if (!bgfx::isValid(g_foundObjectBGTexture))
        return;

    bgfx::VertexLayout layout;
    layout
        .begin()
        .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
        .end();

    bgfx::TransientVertexBuffer transientBuffer;
    bgfx::allocTransientVertexBuffer(&transientBuffer, 6, layout);

    struct sVertice
    {
        float position[3];
        float texcoord[2];
    };

    sVertice* pVertices = (sVertice*)transientBuffer.data;

    // Full-screen quad (0,0)-(320,200), Z=999 (in front of background at 1000)
    pVertices->position[0] = 0.f;   pVertices->position[1] = 0.f;   pVertices->position[2] = 999.f;
    pVertices->texcoord[0] = 0.f;   pVertices->texcoord[1] = 0.f;
    pVertices++;

    pVertices->position[0] = 320.f; pVertices->position[1] = 200.f; pVertices->position[2] = 999.f;
    pVertices->texcoord[0] = 1.f;   pVertices->texcoord[1] = 1.f;
    pVertices++;

    pVertices->position[0] = 320.f; pVertices->position[1] = 0.f;   pVertices->position[2] = 999.f;
    pVertices->texcoord[0] = 1.f;   pVertices->texcoord[1] = 0.f;
    pVertices++;

    pVertices->position[0] = 0.f;   pVertices->position[1] = 0.f;   pVertices->position[2] = 999.f;
    pVertices->texcoord[0] = 0.f;   pVertices->texcoord[1] = 0.f;
    pVertices++;

    pVertices->position[0] = 0.f;   pVertices->position[1] = 200.f; pVertices->position[2] = 999.f;
    pVertices->texcoord[0] = 0.f;   pVertices->texcoord[1] = 1.f;
    pVertices++;

    pVertices->position[0] = 320.f; pVertices->position[1] = 200.f; pVertices->position[2] = 999.f;
    pVertices->texcoord[0] = 1.f;   pVertices->texcoord[1] = 1.f;
    pVertices++;

    static bgfx::UniformHandle foundObjBGTextureUniform = BGFX_INVALID_HANDLE;
    if (!bgfx::isValid(foundObjBGTextureUniform))
    {
        foundObjBGTextureUniform = bgfx::createUniform("s_backgroundTexture", bgfx::UniformType::Sampler);
    }
    static bgfx::UniformHandle foundObjFadeLevelUniform = BGFX_INVALID_HANDLE;
    if (!bgfx::isValid(foundObjFadeLevelUniform))
    {
        foundObjFadeLevelUniform = bgfx::createUniform("u_fadeLevel", bgfx::UniformType::Vec4);
    }

    bgfx::setState(0 | BGFX_STATE_WRITE_RGB
        | BGFX_STATE_WRITE_A
        | BGFX_STATE_MSAA
        | BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_INV_SRC_ALPHA)
    );

    bgfx::setVertexBuffer(0, &transientBuffer);
    bgfx::setTexture(0, foundObjBGTextureUniform, g_foundObjectBGTexture);

    float fadeParams[4] = { g_fadeLevel, 0.0f, 0.0f, 0.0f };
    bgfx::setUniform(foundObjFadeLevelUniform, fadeParams);

    bgfx::submit(gameViewId, getHDBackgroundShader());
}
void osystem_drawBlackScreen()
{
    bgfx::VertexLayout layout;
    layout
        .begin()
        .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
        .end();

    bgfx::TransientVertexBuffer transientBuffer;
    bgfx::allocTransientVertexBuffer(&transientBuffer, 6, layout);

    struct sVertice
    {
        float position[3];
        uint32_t color;
    };

    sVertice* pVertices = (sVertice*)transientBuffer.data;

    // Full-screen black quad at Z=100 (in front of 3D scene but behind UI)
    uint32_t blackColor = 0xFF000000;

    // Triangle 1
    pVertices->position[0] = 0.f;   pVertices->position[1] = 0.f;   pVertices->position[2] = 100.f;
    pVertices->color = blackColor;
    pVertices++;

    pVertices->position[0] = 320.f; pVertices->position[1] = 200.f; pVertices->position[2] = 100.f;
    pVertices->color = blackColor;
    pVertices++;

    pVertices->position[0] = 320.f; pVertices->position[1] = 0.f;   pVertices->position[2] = 100.f;
    pVertices->color = blackColor;
    pVertices++;

    // Triangle 2
    pVertices->position[0] = 0.f;   pVertices->position[1] = 0.f;   pVertices->position[2] = 100.f;
    pVertices->color = blackColor;
    pVertices++;

    pVertices->position[0] = 0.f;   pVertices->position[1] = 200.f; pVertices->position[2] = 100.f;
    pVertices->color = blackColor;
    pVertices++;

    pVertices->position[0] = 320.f; pVertices->position[1] = 200.f; pVertices->position[2] = 100.f;
    pVertices->color = blackColor;
    pVertices++;

    bgfx::setState(0 | BGFX_STATE_WRITE_RGB
        | BGFX_STATE_WRITE_A
        | BGFX_STATE_MSAA
    );

    bgfx::setVertexBuffer(0, &transientBuffer);
    bgfx::submit(gameViewId, getFlatShader());
}

void osystem_drawBackground()
{
    if (backgroundMode == backgroundModeEnum_2D)
    {
        bgfx::VertexLayout layout;
        layout
            .begin()
            .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
            .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
            .end();

        bgfx::TransientVertexBuffer transientBuffer;
        bgfx::allocTransientVertexBuffer(&transientBuffer, 6, layout);

        struct sVertice
        {
            float position[3];
            float texcoord[2];
        };

        sVertice* pVertices = (sVertice*)transientBuffer.data;

        float quadVertices[6 * 3];
        float quadUV[6 * 2];

        float shakeX = g_shakeOffsetX;
        float shakeY = g_shakeOffsetY;

        // 0
        pVertices->position[0] = 0.f + shakeX;
        pVertices->position[1] = 0.f + shakeY;
        pVertices->position[2] = 1000.f;
        pVertices->texcoord[0] = 0.f;
        pVertices->texcoord[1] = 0.f;
        pVertices++;

        //2
        pVertices->position[0] = 320.f + shakeX;
        pVertices->position[1] = 200.f + shakeY;
        pVertices->position[2] = 1000.f;
        pVertices->texcoord[0] = 1.f;
        pVertices->texcoord[1] = 1.f;
        pVertices++;

        //1
        pVertices->position[0] = 320.f + shakeX;
        pVertices->position[1] = 0.f + shakeY;
        pVertices->position[2] = 1000.f;
        pVertices->texcoord[0] = 1.f;
        pVertices->texcoord[1] = 0.f;
        pVertices++;

        //------------------------
        //3
        pVertices->position[0] = 0.f + shakeX;
        pVertices->position[1] = 0.f + shakeY;
        pVertices->position[2] = 1000.f;
        pVertices->texcoord[0] = 0.f;
        pVertices->texcoord[1] = 0.f;
        pVertices++;

        //4
        pVertices->position[0] = 0.f + shakeX;
        pVertices->position[1] = 200.f + shakeY;
        pVertices->position[2] = 1000.f;
        pVertices->texcoord[0] = 0.f;
        pVertices->texcoord[1] = 1.f;
        pVertices++;

        //5
        pVertices->position[0] = 320.f + shakeX;
        pVertices->position[1] = 200.f + shakeY;
        pVertices->position[2] = 1000.f;
        pVertices->texcoord[0] = 1.f;
        pVertices->texcoord[1] = 1.f;
        pVertices++;

        static bgfx::UniformHandle backgroundTextureUniform = BGFX_INVALID_HANDLE;
        if (!bgfx::isValid(backgroundTextureUniform))
        {
            backgroundTextureUniform = bgfx::createUniform("s_backgroundTexture", bgfx::UniformType::Sampler);
        }
        static bgfx::UniformHandle paletteTextureUniform = BGFX_INVALID_HANDLE;
        if (!bgfx::isValid(paletteTextureUniform))
        {
            paletteTextureUniform = bgfx::createUniform("s_paletteTexture", bgfx::UniformType::Sampler);
        }
        static bgfx::UniformHandle fadeLevelUniform = BGFX_INVALID_HANDLE;
        if (!bgfx::isValid(fadeLevelUniform))
        {
            fadeLevelUniform = bgfx::createUniform("u_fadeLevel", bgfx::UniformType::Vec4);
        }


        bgfx::setState(0 | BGFX_STATE_WRITE_RGB
            | BGFX_STATE_MSAA
        );

        bgfx::setVertexBuffer(0, &transientBuffer);

        bgfx::setTexture(0, backgroundTextureUniform, getActiveBackgroundTexture());
        bgfx::setTexture(1, paletteTextureUniform, g_paletteTexture);

        // Set fade level uniform for HD backgrounds
        float fadeParams[4] = { g_fadeLevel, 0.0f, 0.0f, 0.0f };
        bgfx::setUniform(fadeLevelUniform, fadeParams);

        bgfx::submit(gameViewId, g_currentBackgroundIsHD ? getHDBackgroundShader() : getBackgroundShader());


    }
}

bool g_bgfxMainResourcesInitialized = false;

// Controller hint overlay texture for startup menu
static bgfx::TextureHandle g_controllerHintTexture = BGFX_INVALID_HANDLE;
static bool g_controllerHintLoaded = false;
static int g_controllerHintWidth = 0;
static int g_controllerHintHeight = 0;

extern "C" { extern char homePath[512]; }

static void loadControllerHintTexture()
{
    if (g_controllerHintLoaded)
        return;

    int width, height, channels;
    unsigned char* data = nullptr;

    // Try embedded data first
    const unsigned char* embData = nullptr;
    size_t embSize = 0;
    if (getEmbeddedFile("ControllerHint.png", &embData, &embSize))
    {
        data = loadImageFromMemory(embData, (int)embSize, &width, &height, &channels);
    }

    // Fallback to external file
    if (!data)
    {
        char filePath[512];
        snprintf(filePath, sizeof(filePath), "%sControllerHint.png", homePath);
        data = loadImageFile(filePath, &width, &height, &channels);
    }

    if (!data)
    {
        printf(RBGFX_WARN "Controller hint image not found (embedded or filesystem)" CON_RESET "\n");
        g_controllerHintLoaded = true;
        return;
    }

    // Convert to RGBA if needed
    unsigned char* rgbaData = data;
    bool needsFree = false;
    if (channels == 3)
    {
        rgbaData = (unsigned char*)malloc(width * height * 4);
        if (rgbaData)
        {
            for (int p = 0; p < width * height; p++)
            {
                rgbaData[p * 4 + 0] = data[p * 3 + 0];
                rgbaData[p * 4 + 1] = data[p * 3 + 1];
                rgbaData[p * 4 + 2] = data[p * 3 + 2];
                rgbaData[p * 4 + 3] = 255;
            }
            needsFree = true;
        }
    }

    if (rgbaData)
    {
        g_controllerHintTexture = bgfx::createTexture2D(width, height, false, 1,
            bgfx::TextureFormat::RGBA8, BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP,
            bgfx::copy(rgbaData, width * height * 4));
        g_controllerHintWidth = width;
        g_controllerHintHeight = height;
        printf(RBGFX_OK "Controller hint image loaded: %dx%d\n", width, height);
    }

    if (needsFree)
        free(rgbaData);
    freeHDImageData(data);

    g_controllerHintLoaded = true;
}

void osystem_drawControllerHint(float bottomMargin)
{
    // Only show the controller hint if a game controller is actually connected
    if (!g_controllerState.connected)
        return;

    loadControllerHintTexture();

    if (!bgfx::isValid(g_controllerHintTexture))
        return;

    // Get display size from ImGui (matches the window pixel size)
    ImVec2 displaySize = ImGui::GetIO().DisplaySize;
    if (displaySize.x <= 0 || displaySize.y <= 0)
        return;

    // Scale image to fit at the bottom, maintaining aspect ratio
    float imgAspect = (float)g_controllerHintWidth / (float)g_controllerHintHeight;
    float drawWidth = displaySize.x * 0.28f;
    float drawHeight = drawWidth / imgAspect;

    // Position: centered horizontally, at the bottom with a small margin
    float margin = bottomMargin;
    float posX = (displaySize.x - drawWidth) * 0.5f;
    float posY = displaySize.y - drawHeight - margin;

    // Draw as an ImGui overlay window (transparent, no decorations)
    ImGui::SetNextWindowPos(ImVec2(posX, posY));
    ImGui::SetNextWindowSize(ImVec2(drawWidth, drawHeight));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
    ImGui::Begin("##ControllerHint", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_AlwaysAutoResize);

    ImGui::Image(g_controllerHintTexture, ImVec2(drawWidth, drawHeight));

    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
}

void osystem_drawVignette()
{
    if (!g_remasterConfig.postProcessing.enableVignette)
        return;

    float intensity = g_remasterConfig.postProcessing.vignetteIntensity;
    float radius = g_remasterConfig.postProcessing.vignetteRadius;

    if (intensity <= 0.0f)
        return;

    ImVec2 displaySize = ImGui::GetIO().DisplaySize;
    if (displaySize.x <= 0 || displaySize.y <= 0)
        return;

    ImDrawList* drawList = ImGui::GetForegroundDrawList();

    // Draw radial vignette using concentric rectangles with increasing opacity
    // towards the edges. This creates a smooth darkened border effect.
    const int numSteps = 16;
    float maxAlpha = intensity * 255.0f;
    if (maxAlpha > 200.0f) maxAlpha = 200.0f;

    float cx = displaySize.x * 0.5f;
    float cy = displaySize.y * 0.5f;

    for (int i = numSteps; i >= 1; i--)
    {
        float t = (float)i / (float)numSteps;
        // Map t through radius parameter: lower radius = more vignette coverage
        float edgeFactor = (t - radius) / (1.0f - radius);
        if (edgeFactor <= 0.0f)
            continue;

        // Smooth ease-in curve for natural falloff
        float alpha = edgeFactor * edgeFactor * maxAlpha;
        if (alpha < 1.0f)
            continue;

        ImU32 color = IM_COL32(0, 0, 0, (int)alpha);

        // Calculate rect bounds for this step (inset from screen edges)
        float insetX = cx * (1.0f - t);
        float insetY = cy * (1.0f - t);

        // Draw 4 edge strips (top, bottom, left, right)
        float nextInsetX = (i > 1) ? cx * (1.0f - (float)(i - 1) / (float)numSteps) : 0.0f;
        float nextInsetY = (i > 1) ? cy * (1.0f - (float)(i - 1) / (float)numSteps) : 0.0f;

        // Top strip
        drawList->AddRectFilled(
            ImVec2(nextInsetX, nextInsetY),
            ImVec2(displaySize.x - nextInsetX, insetY),
            color);
        // Bottom strip
        drawList->AddRectFilled(
            ImVec2(nextInsetX, displaySize.y - insetY),
            ImVec2(displaySize.x - nextInsetX, displaySize.y - nextInsetY),
            color);
        // Left strip
        drawList->AddRectFilled(
            ImVec2(nextInsetX, insetY),
            ImVec2(insetX, displaySize.y - insetY),
            color);
        // Right strip
        drawList->AddRectFilled(
            ImVec2(displaySize.x - insetX, insetY),
            ImVec2(displaySize.x - nextInsetX, displaySize.y - insetY),
            color);
    }
}

bgfx::FrameBufferHandle fieldModelInspector_FB = BGFX_INVALID_HANDLE;
bgfx::TextureHandle fieldModelInspector_Texture = BGFX_INVALID_HANDLE;
bgfx::TextureHandle fieldModelInspector_Depth = BGFX_INVALID_HANDLE;

void initBgfxMainResources()
{
    // create background texture
    g_backgroundTexture = bgfx::createTexture2D(320, 200, false, 1, bgfx::TextureFormat::R8U);
    g_uiLayerTexture = bgfx::createTexture2D(320, 200, false, 1, bgfx::TextureFormat::R8U);
    g_paletteTexture = bgfx::createTexture2D(3, 256, false, 1, bgfx::TextureFormat::R8U);
}

ImVec2 gameResolution = { 320, 200 };

void renderGameWindow()
{
    if (ImGui::Begin("Game"))
    {
        if (bgfx::getCaps()->originBottomLeft)
        {
            ImGui::Image(fieldModelInspector_Texture, gameResolution, ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));
        }
        else
        {
            ImGui::Image(fieldModelInspector_Texture, gameResolution, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f));
        }
    }
    ImGui::End();
}

void osystem_startFrame()
{
    if (!g_bgfxMainResourcesInitialized)
    {
        initBgfxMainResources();
        g_bgfxMainResourcesInitialized = true;
    }

    // Process deferred resource frees
    ResourceGC::tick();

    // Update animated HD background
    static u32 lastUpdateTime = 0;
    u32 currentTime = SDL_GetTicks();
    if (lastUpdateTime == 0)
    {
        lastUpdateTime = currentTime;
    }
    float deltaTime = (currentTime - lastUpdateTime) / 1000.0f;
    lastUpdateTime = currentTime;

    updateAnimatedHDBackground(deltaTime);

    //uiLayer.fill(0x0);

    static ImVec2 oldWindowSize = { -1,-1 };

    if (debuggerVar_debugMenuDisplayed)
    {
        gameViewId = 1;
        if (ImGui::Begin("Game"))
        {
            ImVec2 currentWindowSize = ImGui::GetContentRegionAvail();

            currentWindowSize[0] = std::max<int>(currentWindowSize[0], 1);
            currentWindowSize[1] = std::max<int>(currentWindowSize[1], 1);

            gameResolution = currentWindowSize;
        }
        else
        {
            gameResolution = { 320, 200 };
        }
        ImGui::End();

        if ((gameResolution[0] != oldWindowSize[0]) || (gameResolution[1] != oldWindowSize[1]))
        {
            oldWindowSize = gameResolution;

            if (bgfx::isValid(fieldModelInspector_FB))
            {
                bgfx::destroy(fieldModelInspector_FB);
            }

            const uint64_t tsFlags = 0
                //| BGFX_SAMPLER_MIN_POINT
                //| BGFX_SAMPLER_MAG_POINT
                //| BGFX_SAMPLER_MIP_POINT
                | BGFX_SAMPLER_U_CLAMP
                | BGFX_SAMPLER_V_CLAMP
                ;

            fieldModelInspector_Texture = bgfx::createTexture2D(gameResolution[0], gameResolution[1], false, 0, bgfx::TextureFormat::BGRA8, BGFX_TEXTURE_RT | tsFlags);
            fieldModelInspector_Depth = bgfx::createTexture2D(gameResolution[0], gameResolution[1], false, 0, bgfx::TextureFormat::D24S8, BGFX_TEXTURE_RT | tsFlags);
            std::array<bgfx::Attachment, 2> attachements;
            attachements[0].init(fieldModelInspector_Texture);
            attachements[1].init(fieldModelInspector_Depth);
            fieldModelInspector_FB = bgfx::createFrameBuffer(2, &attachements[0], true);
        }
        bgfx::setViewFrameBuffer(gameViewId, fieldModelInspector_FB);
        bgfx::setViewRect(gameViewId, 0, 0, gameResolution[0], gameResolution[1]);
    }
    else
    {
        gameViewId = 0;
        gameResolution[0] = outputResolution[0];
        gameResolution[1] = outputResolution[1];
        // beginScene() always binds the offscreen FB to view 0 now
        // (no backbuffer fallback needed; PP pipeline is always active for scene capture)
    }

    {
        bgfx::setViewClear(gameViewId, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH | BGFX_CLEAR_STENCIL, 255);

        bgfx::setViewName(gameViewId, "Game");
        bgfx::setViewMode(gameViewId, bgfx::ViewMode::Sequential);

        bgfx::touch(gameViewId);
    }

    osystem_drawBackground();

    if (g_portraitOverlayChoice >= 0)
    {
        osystem_drawPortraitOverlay(g_portraitOverlayChoice);
    }
}

unsigned char frontBuffer[320 * 200];
unsigned char physicalScreen[320 * 200];
std::array<unsigned char, 320 * 200> uiLayer;

void osystem_CopyBlockPhys(unsigned char* videoBuffer, int left, int top, int right, int bottom)
{
    unsigned char* in = (unsigned char*)&videoBuffer[0] + left + top * 320;

    while ((right - left) % 4)
    {
        right++;
    }

    while ((bottom - top) % 4)
    {
        bottom++;
    }

    // Clamp to screen bounds after alignment rounding
    if (right > 320) right = 320;
    if (bottom > 200) bottom = 200;

    for (int i = top; i < bottom; i++)
    {
        in = (unsigned char*)&videoBuffer[0] + left + i * 320;
        unsigned char* out2 = physicalScreen + left + i * 320;
        for (int j = left; j < right; j++)
        {
            *(out2++) = *(in++);
        }
    }

    bgfx::updateTexture2D(g_backgroundTexture, 0, 0, 0, 0, 320, 200, bgfx::copy(physicalScreen, 320 * 200));
}

void osystem_refreshFrontTextureBuffer()
{}

void osystem_initBuffer()
{
    memset(backBuffer, 0x0, 512 * 256 * 3);
}

void gameScreenToViewport(float* X, float* Y)
{
    (*X) = (*X) * g_screenWidth / 320.f;
    (*Y) = (*Y) * g_screenHeight / 200.f;

    (*Y) = g_screenHeight - (*Y);
}

void osystem_setClip(float left, float top, float right, float bottom)
{
    float x1 = left - 1;
    float y1 = bottom + 1;
    float x2 = right + 1;
    float y2 = top - 1;

    gameScreenToViewport(&x1, &y1);
    gameScreenToViewport(&x2, &y2);

    float width = x2 - x1;
    float height = y2 - y1;

    float currentScissor[4];
    currentScissor[0] = ((left - 1) / 320.f) * gameResolution[0];
    currentScissor[1] = ((top - 1) / 200.f) * gameResolution[1];
    currentScissor[2] = ((right - left + 2) / 320.f) * gameResolution[0];
    currentScissor[3] = ((bottom - top + 2) / 200.f) * gameResolution[1];

    currentScissor[0] = std::max<float>(currentScissor[0], 0);
    currentScissor[1] = std::max<float>(currentScissor[1], 0);

    bgfx::setScissor(currentScissor[0], currentScissor[1], currentScissor[2], currentScissor[3]);
}

void osystem_clearClip()
{
    bgfx::setScissor(0, 0, gameResolution[0], gameResolution[1]);
}

void osystem_stopFrame()
{}

void osystem_flushPendingPrimitives()
{
    // Helper lambda to apply shake offset to polyVertex arrays in transient buffers
    auto applyShakeToVertices = [](uint8_t* data, int numVertices, int stride) {
        if (g_shakeOffsetX == 0.f && g_shakeOffsetY == 0.f) return;
        for (int i = 0; i < numVertices; i++) {
            float* pos = (float*)(data + i * stride);
            pos[0] += g_shakeOffsetX;
            pos[1] += g_shakeOffsetY;
        }
        };

    if (numUsedFlatVertices)
    {
        bgfx::VertexLayout layout;
        layout
            .begin()
            .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
            .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
            .end();

        bgfx::TransientVertexBuffer transientBuffer;
        bgfx::allocTransientVertexBuffer(&transientBuffer, numUsedFlatVertices, layout);

        memcpy(transientBuffer.data, &flatVertices[0], sizeof(polyVertex) * numUsedFlatVertices);
        applyShakeToVertices(transientBuffer.data, numUsedFlatVertices, sizeof(polyVertex));

        bgfx::setState(0 | BGFX_STATE_WRITE_RGB
            | BGFX_STATE_WRITE_A
            | BGFX_STATE_WRITE_Z
            | BGFX_STATE_DEPTH_TEST_LEQUAL
            | BGFX_STATE_MSAA
            | BGFX_STATE_CULL_CCW
        );

        // If flushing shadow, test stencil==0 and increment on pass to prevent overdraw
        if (g_submitShadowStencil)
        {
            bgfx::setStencil(BGFX_STENCIL_TEST_EQUAL
                | BGFX_STENCIL_FUNC_REF(0)
                | BGFX_STENCIL_FUNC_RMASK(0xFF)
                | BGFX_STENCIL_OP_FAIL_Z_KEEP
                | BGFX_STENCIL_OP_PASS_Z_INCR);
        }

        static bgfx::UniformHandle paletteTextureUniform = BGFX_INVALID_HANDLE;
        if (!bgfx::isValid(paletteTextureUniform))
        {
            paletteTextureUniform = bgfx::createUniform("s_paletteTexture", bgfx::UniformType::Sampler);
        }

        bgfx::setTexture(1, paletteTextureUniform, g_paletteTexture);

        bgfx::setVertexBuffer(0, &transientBuffer);
        bgfx::submit(gameViewId, getFlatShader());
    }

    if (numUsedNoiseVertices)
    {
        bgfx::VertexLayout layout;
        layout
            .begin()
            .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
            .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
            .end();

        bgfx::TransientVertexBuffer transientBuffer;
        bgfx::allocTransientVertexBuffer(&transientBuffer, numUsedNoiseVertices, layout);

        memcpy(transientBuffer.data, &noiseVertices[0], sizeof(polyVertex) * numUsedNoiseVertices);
        applyShakeToVertices(transientBuffer.data, numUsedNoiseVertices, sizeof(polyVertex));

        bgfx::setState(0 | BGFX_STATE_WRITE_RGB
            | BGFX_STATE_WRITE_A
            | BGFX_STATE_WRITE_Z
            | BGFX_STATE_DEPTH_TEST_LEQUAL
            | BGFX_STATE_MSAA
            | BGFX_STATE_CULL_CCW
        );

        static bgfx::UniformHandle paletteTextureUniform = BGFX_INVALID_HANDLE;
        if (!bgfx::isValid(paletteTextureUniform))
        {
            paletteTextureUniform = bgfx::createUniform("s_paletteTexture", bgfx::UniformType::Sampler);
        }

        bgfx::setTexture(1, paletteTextureUniform, g_paletteTexture);

        // noise texture
        static bgfx::TextureHandle noiseTexture = BGFX_INVALID_HANDLE;
        if (!bgfx::isValid(noiseTexture))
        {
            const int noiseTextureDim = 256;
            std::array<u8, noiseTextureDim* noiseTextureDim> noiseTextureData;
            for (int i = 0; i < noiseTextureData.size(); i++) {
                noiseTextureData[i] = rand();
            }
            noiseTexture = bgfx::createTexture2D(noiseTextureDim, noiseTextureDim, false, 1, bgfx::TextureFormat::R8U, 0, bgfx::copy(noiseTextureData.data(), noiseTextureDim * noiseTextureDim));
        }
        static bgfx::UniformHandle noiseTextureUniform = BGFX_INVALID_HANDLE;
        if (!bgfx::isValid(noiseTextureUniform)) {
            noiseTextureUniform = bgfx::createUniform("s_noiseTexture", bgfx::UniformType::Sampler);
        }

        bgfx::setTexture(0, noiseTextureUniform, noiseTexture);

        bgfx::setVertexBuffer(0, &transientBuffer);
        bgfx::submit(gameViewId, getNoiseShader());
    }

    if (numUsedRampVertices)
    {
        bgfx::VertexLayout layout;
        layout
            .begin()
            .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
            .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
            .end();

        bgfx::TransientVertexBuffer transientBuffer;
        bgfx::allocTransientVertexBuffer(&transientBuffer, numUsedRampVertices, layout);

        memcpy(transientBuffer.data, &rampVertices[0], sizeof(polyVertex) * numUsedRampVertices);
        applyShakeToVertices(transientBuffer.data, numUsedRampVertices, sizeof(polyVertex));

        bgfx::setState(0 | BGFX_STATE_WRITE_RGB
            | BGFX_STATE_WRITE_A
            | BGFX_STATE_WRITE_Z
            | BGFX_STATE_DEPTH_TEST_LEQUAL
            | BGFX_STATE_MSAA
            | BGFX_STATE_CULL_CCW
        );

        static bgfx::UniformHandle paletteTextureUniform = BGFX_INVALID_HANDLE;
        if (!bgfx::isValid(paletteTextureUniform))
        {
            paletteTextureUniform = bgfx::createUniform("s_paletteTexture", bgfx::UniformType::Sampler);
        }

        bgfx::setTexture(1, paletteTextureUniform, g_paletteTexture);
        bgfx::setVertexBuffer(0, &transientBuffer);
        bgfx::submit(gameViewId, getRampShader());
    }

    if (numUsedSpheres)
    {
        bgfx::VertexLayout layout;
        layout
            .begin()
            .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
            .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
            .add(bgfx::Attrib::TexCoord1, 4, bgfx::AttribType::Float)
            .end();

        bgfx::TransientVertexBuffer transientBuffer;
        bgfx::allocTransientVertexBuffer(&transientBuffer, numUsedSpheres, layout);

        memcpy(transientBuffer.data, &sphereVertices[0], sizeof(sphereVertex) * numUsedSpheres);
        applyShakeToVertices(transientBuffer.data, numUsedSpheres, sizeof(sphereVertex));

        bgfx::setState(0 | BGFX_STATE_WRITE_RGB
            | BGFX_STATE_WRITE_A
            | BGFX_STATE_WRITE_Z
            | BGFX_STATE_DEPTH_TEST_LEQUAL
            | BGFX_STATE_MSAA
            | BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_INV_SRC_ALPHA)
        );

        // Check if we have a sphere atlas texture for the current model
        ModelAtlasData* currentAtlas = getCurrentAtlas();
        bgfx::TextureHandle sphereTexToUse = g_paletteTexture;  // Default fallback
        float atlasWidth = 0.f;
        float atlasHeight = 0.f;

        if (currentAtlas && bgfx::isValid(currentAtlas->sphereTexture))
        {
            // Use the sphere atlas texture instead of palette
            sphereTexToUse = currentAtlas->sphereTexture;
            atlasWidth = (float)currentAtlas->sphereAtlasWidth;
            atlasHeight = (float)currentAtlas->sphereAtlasHeight;
        }

        // Use s_modelTexture uniform for sphere atlas (sampler slot 0)
        static bgfx::UniformHandle modelTextureUniform = BGFX_INVALID_HANDLE;
        if (!bgfx::isValid(modelTextureUniform))
        {
            modelTextureUniform = bgfx::createUniform("s_modelTexture", bgfx::UniformType::Sampler);
        }

        static bgfx::UniformHandle sphereAtlasInfoUniform = BGFX_INVALID_HANDLE;
        if (!bgfx::isValid(sphereAtlasInfoUniform))
        {
            sphereAtlasInfoUniform = bgfx::createUniform("u_sphereAtlasInfo", bgfx::UniformType::Vec4);
        }

        // Set sphere atlas dimensions uniform (shader uses this to decide palette vs atlas mode)
        float atlasInfo[] = { atlasWidth, atlasHeight, 0.f, 0.f };
        bgfx::setUniform(sphereAtlasInfoUniform, atlasInfo);

        // Bind palette texture at slot 1 for getColor() fallback in palette.sh
        static bgfx::UniformHandle spherePaletteUniform = BGFX_INVALID_HANDLE;
        if (!bgfx::isValid(spherePaletteUniform))
        {
            spherePaletteUniform = bgfx::createUniform("s_paletteTexture", bgfx::UniformType::Sampler);
        }
        bgfx::setTexture(1, spherePaletteUniform, g_paletteTexture);

        bgfx::setTexture(0, modelTextureUniform, sphereTexToUse);
        bgfx::setVertexBuffer(0, &transientBuffer);
        bgfx::submit(gameViewId, getSphereShader());
    }

    if (g_lineVertices.size()) {
        bgfx::VertexLayout layout;
        layout
            .begin()
            .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
            .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
            .end();

        bgfx::TransientVertexBuffer transientBuffer;
        bgfx::allocTransientVertexBuffer(&transientBuffer, g_lineVertices.size(), layout);

        memcpy(transientBuffer.data, g_lineVertices.data(), sizeof(polyVertex) * g_lineVertices.size());
        applyShakeToVertices(transientBuffer.data, g_lineVertices.size(), sizeof(polyVertex));

        bgfx::setState(0 | BGFX_STATE_PT_LINES
            | BGFX_STATE_WRITE_RGB
            | BGFX_STATE_WRITE_A
            | BGFX_STATE_WRITE_Z
            | BGFX_STATE_DEPTH_TEST_LEQUAL
            | BGFX_STATE_MSAA
            | BGFX_STATE_LINEAA
            | BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_INV_SRC_ALPHA)
        );

        static bgfx::UniformHandle paletteTextureUniform = BGFX_INVALID_HANDLE;
        if (!bgfx::isValid(paletteTextureUniform))
        {
            paletteTextureUniform = bgfx::createUniform("s_paletteTexture", bgfx::UniformType::Sampler);
        }

        bgfx::setTexture(1, paletteTextureUniform, g_paletteTexture);

        bgfx::setVertexBuffer(0, &transientBuffer);
        bgfx::submit(gameViewId, getFlatShader());
    }

    if (numUsedTransparentVertices)
    {
        bgfx::VertexLayout layout;
        layout
            .begin()
            .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
            .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
            .end();

        bgfx::TransientVertexBuffer transientBuffer;
        bgfx::allocTransientVertexBuffer(&transientBuffer, numUsedTransparentVertices, layout);

        memcpy(transientBuffer.data, &transparentVertices[0], sizeof(polyVertex) * numUsedTransparentVertices);
        applyShakeToVertices(transientBuffer.data, numUsedTransparentVertices, sizeof(polyVertex));

        bgfx::setState(0 | BGFX_STATE_WRITE_RGB
            | BGFX_STATE_WRITE_A
            | BGFX_STATE_DEPTH_TEST_LEQUAL
            | BGFX_STATE_MSAA
            | BGFX_STATE_CULL_CCW
            | BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_FACTOR, BGFX_STATE_BLEND_INV_FACTOR)
            , 0x80808080);

        // If flushing shadow, test stencil==0 and increment on pass to prevent overdraw
        if (g_submitShadowStencil)
        {
            bgfx::setStencil(BGFX_STENCIL_TEST_EQUAL
                | BGFX_STENCIL_FUNC_REF(0)
                | BGFX_STENCIL_FUNC_RMASK(0xFF)
                | BGFX_STENCIL_OP_FAIL_Z_KEEP
                | BGFX_STENCIL_OP_PASS_Z_INCR);
        }

        static bgfx::UniformHandle paletteTextureUniform = BGFX_INVALID_HANDLE;
        if (!bgfx::isValid(paletteTextureUniform))
        {
            paletteTextureUniform = bgfx::createUniform("s_paletteTexture", bgfx::UniformType::Sampler);
        }

        bgfx::setTexture(1, paletteTextureUniform, g_paletteTexture);
        bgfx::setVertexBuffer(0, &transientBuffer);
        bgfx::submit(gameViewId, getFlatShader());
    }

    if (numUsedTexturedVertices && bgfx::isValid(g_activeModelTexture))
    {
        bgfx::VertexLayout layout;
        layout
            .begin()
            .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
            .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
            .end();

        bgfx::TransientVertexBuffer transientBuffer;
        bgfx::allocTransientVertexBuffer(&transientBuffer, numUsedTexturedVertices, layout);

        memcpy(transientBuffer.data, &texturedVertices[0], sizeof(polyVertex) * numUsedTexturedVertices);
        applyShakeToVertices(transientBuffer.data, numUsedTexturedVertices, sizeof(polyVertex));

        bgfx::setState(0 | BGFX_STATE_WRITE_RGB
            | BGFX_STATE_WRITE_A
            | BGFX_STATE_WRITE_Z
            | BGFX_STATE_DEPTH_TEST_LEQUAL
            | BGFX_STATE_MSAA
            | BGFX_STATE_CULL_CCW
            | BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_INV_SRC_ALPHA)
        );

        static bgfx::UniformHandle modelTextureUniform = BGFX_INVALID_HANDLE;
        if (!bgfx::isValid(modelTextureUniform))
        {
            modelTextureUniform = bgfx::createUniform("s_modelTexture", bgfx::UniformType::Sampler);
        }

        bgfx::setTexture(0, modelTextureUniform, g_activeModelTexture);
        bgfx::setVertexBuffer(0, &transientBuffer);
        bgfx::submit(gameViewId, getTexturedShader());
    }

    numUsedFlatVertices = 0;
    numUsedNoiseVertices = 0;
    numUsedRampVertices = 0;
    numUsedSpheres = 0;
    numSpheresPrimitives = 0;
    numUsedTransparentVertices = 0;
    numUsedTexturedVertices = 0;
    g_lineVertices.clear();
}

void osystem_fillPoly(float* buffer, int numPoint, unsigned char color, u8 polyType)
{
#define MAX_POINTS_PER_POLY 50
    float UVArray[MAX_POINTS_PER_POLY];

    if (numPoint >= MAX_POINTS_PER_POLY || numPoint < 3)
        return;

    // compute the polygon bounding box
    float polyMinX = 320.f;
    float polyMaxX = 0.f;
    float polyMinY = 200.f;
    float polyMaxY = 0.f;

    for (int i = 0; i < numPoint; i++)
    {
        float X = buffer[3 * i + 0];
        float Y = buffer[3 * i + 1];

        if (X > polyMaxX)
            polyMaxX = X;
        if (X < polyMinX)
            polyMinX = X;

        if (Y > polyMaxY)
            polyMaxY = Y;
        if (Y < polyMinY)
            polyMinY = Y;
    }

    float polyWidth = polyMaxX - polyMinX;
    float polyHeight = polyMaxY - polyMinY;

    if (polyWidth <= 0.f)
        polyWidth = 1;
    if (polyHeight <= 0.f)
        polyHeight = 1;

    switch (polyType)
    {
    default:
    case 0: // flat (triste)
    {
        int needed = (numPoint - 2) * 3;
        if (numUsedFlatVertices + needed > NUM_MAX_FLAT_VERTICES)
            return;
        polyVertex* pVertex = &flatVertices[numUsedFlatVertices];
        numUsedFlatVertices += needed;

        for (int i = 0; i < numPoint; i++)
        {
            if (i >= 3)
            {
                memcpy(pVertex, &pVertex[-3], sizeof(polyVertex));
                pVertex++;
                memcpy(pVertex, &pVertex[-2], sizeof(polyVertex));
                pVertex++;
            }

            pVertex->X = buffer[i * 3 + 0];
            pVertex->Y = buffer[i * 3 + 1];
            pVertex->Z = buffer[i * 3 + 2];

            int bank = (color & 0xF0) >> 4;
            int startColor = color & 0xF;
            float colorf = startColor;
            pVertex->U = colorf / 15.f;
            pVertex->V = bank / 15.f;
            pVertex++;
        }
        break;
    }
    case 1: // dither (pierre/tele) - use flat shader instead of noise to avoid transparency
    {
        int needed = (numPoint - 2) * 3;
        if (numUsedFlatVertices + needed > NUM_MAX_FLAT_VERTICES)
            return;
        polyVertex* pVertex = &flatVertices[numUsedFlatVertices];
        numUsedFlatVertices += needed;

        for (int i = 0; i < numPoint; i++)
        {
            if (i >= 3)
            {
                memcpy(pVertex, &pVertex[-3], sizeof(polyVertex));
                pVertex++;
                memcpy(pVertex, &pVertex[-2], sizeof(polyVertex));
                pVertex++;
            }

            pVertex->X = buffer[i * 3 + 0];
            pVertex->Y = buffer[i * 3 + 1];
            pVertex->Z = buffer[i * 3 + 2];

            int bank = (color & 0xF0) >> 4;
            int startColor = color & 0xF;
            float colorf = startColor;
            pVertex->U = colorf / 15.f;
            pVertex->V = bank / 15.f;
            pVertex++;
        }
        break;
    }
    case 2: // trans
    {
        int needed = (numPoint - 2) * 3;
        if (numUsedTransparentVertices + needed > NUM_MAX_TRANSPARENT_VERTICES)
            return;
        polyVertex* pVertex = &transparentVertices[numUsedTransparentVertices];
        numUsedTransparentVertices += needed;

        for (int i = 0; i < numPoint; i++)
        {
            if (i >= 3)
            {
                memcpy(pVertex, &pVertex[-3], sizeof(polyVertex));
                pVertex++;
                memcpy(pVertex, &pVertex[-2], sizeof(polyVertex));
                pVertex++;
            }

            pVertex->X = buffer[i * 3 + 0];
            pVertex->Y = buffer[i * 3 + 1];
            pVertex->Z = buffer[i * 3 + 2];

            int bank = (color & 0xF0) >> 4;
            int startColor = color & 0xF;
            float colorf = startColor;
            pVertex->U = colorf / 15.f;
            pVertex->V = bank / 15.f;
            pVertex++;
        }
        break;
    }
    case 4: // copper (ramps top to bottom)
    case 5: // copper2 (ramps top to bottom, 2 scanline per color)
    {
        int needed = (numPoint - 2) * 3;
        if (numUsedRampVertices + needed > NUM_MAX_RAMP_VERTICES)
            return;
        polyVertex* pVertex = &rampVertices[numUsedRampVertices];
        numUsedRampVertices += needed;

        int bank = (color & 0xF0) >> 4;
        int startColor = color & 0xF;
        float colorStep = 1; // TODO: this should be the scanline ratio for the current resolution to original resolution
        if (polyType == 5)
        {
            colorStep *= 0.5; // to stretch the ramp by 2 for copper2
        }

        for (int i = 0; i < numPoint; i++)
        {
            if (i >= 3)
            {
                memcpy(pVertex, &pVertex[-3], sizeof(polyVertex));
                pVertex++;
                memcpy(pVertex, &pVertex[-2], sizeof(polyVertex));
                pVertex++;
            }

            pVertex->X = buffer[i * 3 + 0];
            pVertex->Y = buffer[i * 3 + 1];
            pVertex->Z = buffer[i * 3 + 2];

            float colorf = startColor + colorStep * (pVertex->Y - polyMinY);

            pVertex->U = colorf / 15.f;
            pVertex->V = bank / 15.f;
            pVertex++;
        }
        break;
    }
    case 3: // marbre (ramp left to right)
    {
        int needed = (numPoint - 2) * 3;
        if (numUsedRampVertices + needed > NUM_MAX_RAMP_VERTICES)
            return;
        polyVertex* pVertex = &rampVertices[numUsedRampVertices];
        numUsedRampVertices += needed;

        float colorStep = 15.f / polyWidth;

        int bank = (color & 0xF0) >> 4;
        int startColor = color & 0xF;

        assert(startColor == 0);

        for (int i = 0; i < numPoint; i++)
        {
            if (i >= 3)
            {
                memcpy(pVertex, &pVertex[-3], sizeof(polyVertex));
                pVertex++;
                memcpy(pVertex, &pVertex[-2], sizeof(polyVertex));
                pVertex++;
            }

            pVertex->X = buffer[i * 3 + 0];
            pVertex->Y = buffer[i * 3 + 1];
            pVertex->Z = buffer[i * 3 + 2];

            float colorf = startColor + colorStep * (pVertex->X - polyMinX);

            pVertex->U = colorf / 15.f;
            pVertex->V = bank / 15.f;
            pVertex++;
        }
        break;
    }
    case 6: // marbre2 (ramp right to left)
    {
        int needed = (numPoint - 2) * 3;
        if (numUsedRampVertices + needed > NUM_MAX_RAMP_VERTICES)
            return;
        polyVertex* pVertex = &rampVertices[numUsedRampVertices];
        numUsedRampVertices += needed;

        float colorStep = 15.f / polyWidth;

        int bank = (color & 0xF0) >> 4;
        int startColor = color & 0xF;

        assert(startColor == 0);

        for (int i = 0; i < numPoint; i++)
        {
            if (i >= 3)
            {
                memcpy(pVertex, &pVertex[-3], sizeof(polyVertex));
                pVertex++;
                memcpy(pVertex, &pVertex[-2], sizeof(polyVertex));
                pVertex++;
            }

            pVertex->X = buffer[i * 3 + 0];
            pVertex->Y = buffer[i * 3 + 1];
            pVertex->Z = buffer[i * 3 + 2];

            float colorf = startColor + colorStep * (pVertex->X - polyMinX);

            pVertex->U = 1.f - colorf / 15.f;
            pVertex->V = bank / 15.f;
            pVertex++;
        }
        break;
    }
    }
}

void osystem_fillPolyTextured(float* buffer, int numPoint, float* uvs, bgfx::TextureHandle texture)
{
    if (numPoint < 3 || numPoint >= 50)
        return;

    int needed = (numPoint - 2) * 3;
    if (numUsedTexturedVertices + needed > NUM_MAX_TEXTURED_VERTICES)
        return;

    g_activeModelTexture = texture;

    polyVertex* pVertex = &texturedVertices[numUsedTexturedVertices];
    numUsedTexturedVertices += needed;

    for (int i = 0; i < numPoint; i++)
    {
        if (i >= 3)
        {
            memcpy(pVertex, &pVertex[-3], sizeof(polyVertex));
            pVertex++;
            memcpy(pVertex, &pVertex[-2], sizeof(polyVertex));
            pVertex++;
        }

        pVertex->X = buffer[i * 3 + 0];
        pVertex->Y = buffer[i * 3 + 1];
        pVertex->Z = buffer[i * 3 + 2];

        pVertex->U = uvs[i * 2 + 0];
        pVertex->V = uvs[i * 2 + 1];
        pVertex++;
    }
}

void osystem_draw3dLine(float x1, float y1, float z1, float x2, float y2, float z2, unsigned char color)
{
    polyVertex vertex1;
    polyVertex vertex2;

    vertex1.X = x1;
    vertex1.Y = y1;
    vertex1.Z = z1;

    vertex2.X = x2;
    vertex2.Y = y2;
    vertex2.Z = z2;

    int bank = (color & 0xF0) >> 4;
    int startColor = color & 0xF;
    float colorf = startColor;
    vertex1.U = vertex2.U = colorf / 15.f;
    vertex1.V = vertex2.V = bank / 15.f;

    g_lineVertices.push_back(vertex1);
    g_lineVertices.push_back(vertex2);
}

void osystem_draw3dQuad(float x1, float y1, float z1, float x2, float y2, float z2, float x3, float y3, float z3, float x4, float y4, float z4, unsigned char color, int transparency)
{
    osystem_draw3dLine(x1, y1, z1, x2, y2, z2, color);
    osystem_draw3dLine(x2, y2, z2, x3, y3, z3, color);
    osystem_draw3dLine(x3, y3, z3, x4, y4, z4, color);
    osystem_draw3dLine(x4, y4, z4, x1, y1, z1, color);
}

void osystem_drawSphere(float X, float Y, float Z, u8 color, u8 material, float size)
{
    osystem_drawPoint(X, Y, Z, color, material, size);
}

void osystem_drawPoint(float X, float Y, float Z, u8 color, u8 material, float size)
{
    float fScaleRatio = 6.f / 5.f;
    std::array<sphereVertex, 4> corners;
    corners[0].X = X + size * fScaleRatio;
    corners[0].Y = Y + size * fScaleRatio;
    corners[0].Z = Z;

    corners[1].X = X + size * fScaleRatio;
    corners[1].Y = Y - size * fScaleRatio;
    corners[1].Z = Z;

    corners[2].X = X - size * fScaleRatio;
    corners[2].Y = Y - size * fScaleRatio;
    corners[2].Z = Z;

    corners[3].X = X - size * fScaleRatio;
    corners[3].Y = Y + size * fScaleRatio;
    corners[3].Z = Z;

    std::array<int, 2 * 3> mapping = { {
            0,1,2,
            0,2,3
    } };

    for (int i = 0; i < mapping.size(); i++)
    {
        if (numUsedSpheres >= NUM_MAX_SPHERES_VERTICES)
        {
            assert(false && "numUsedSpheres >= NUM_MAX_SPHERES_VERTICES");
            return;
        }

        sphereVertex* pVertex = &sphereVertices[numUsedSpheres];
        numUsedSpheres++;

        pVertex->X = corners[mapping[i]].X;
        pVertex->Y = corners[mapping[i]].Y;
        pVertex->Z = corners[mapping[i]].Z;

        // Compute UV from sphere atlas grid cell position
        // Sphere atlas layout: 8 columns, 128x128 pixel cells per sphere
        ModelAtlasData* atlasData = getCurrentAtlas();
        if (atlasData && atlasData->sphereAtlasWidth > 0 && atlasData->sphereAtlasHeight > 0)
        {
            const int CELLS_PER_ROW = 8;
            const int CELL_SIZE_PIXELS = 128;

            int cellX = (numSpheresPrimitives - 1) % CELLS_PER_ROW;
            int cellY = (numSpheresPrimitives - 1) / CELLS_PER_ROW;

            float cellPixelX = (float)(cellX * CELL_SIZE_PIXELS);
            float cellPixelY = (float)(cellY * CELL_SIZE_PIXELS);

            // Convert corner local UV (0-1) to atlas cell space
            // Get corner local coords (normalized in -1 to 1 range from sphere center)
            float cornerLocalU = (corners[mapping[i]].X - X) / (size * 1.2f) * 0.5f + 0.5f; // Normalize to 0-1
            float cornerLocalV = (corners[mapping[i]].Y - Y) / (size * 1.2f) * 0.5f + 0.5f;

            // Map to cell space
            pVertex->U = (cellPixelX + cornerLocalU * CELL_SIZE_PIXELS) / atlasData->sphereAtlasWidth;
            pVertex->V = (cellPixelY + cornerLocalV * CELL_SIZE_PIXELS) / atlasData->sphereAtlasHeight;
        }
        else
        {
            // Encode palette color into UV for the shader's palette lookup
            int bank = (color >> 4) & 0xF;
            int startColor = color & 0xF;
            pVertex->U = (float)startColor / 15.f;
            pVertex->V = (float)bank / 15.f;
        }

        pVertex->size = size;
        pVertex->centerX = X;
        pVertex->centerY = Y;
        pVertex->material = material;
    }
}

void osystem_flip(unsigned char* videoBuffer)
{
    osystem_flushPendingPrimitives();
}

// ── Mask dump / replacement helpers ─────────────────────────────────────────
// Dump: upscales the 320x200 mask to HD background resolution and writes it
//       to masks_dump/ as a PNG so artists can edit at full detail.
// Load: checks masks_hd/ for a hand-edited replacement PNG and returns it at
//       its native resolution (HD).  The GPU texture is created at that size;
//       the shader samples via UVs so any resolution works.
// ─────────────────────────────────────────────────────────────────────────────

static void ensureMaskDirectory(const char* dir)
{
#ifdef _WIN32
    _mkdir(dir);
#else
    mkdir(dir, 0755);
#endif
}

static void buildMaskFilename(char* buf, size_t bufSize, const char* folder, int roomId, int actualRoomNumber, int maskId)
{
    // Organise by floor/camera/room so masks are uniquely identified:
    //   <folder>/FLOOR##/CAM###/RID###_ROOM##_MASK##.png
    // roomId is the internal room index (unique identifier in maskTextures array)
    // actualRoomNumber is the real room number (not the camera-relative index)
    // maskId is the mask index within the room
    char floorDir[512];
    snprintf(floorDir, sizeof(floorDir), "%s/FLOOR%02d", folder, (int)g_currentFloor);
    ensureMaskDirectory(floorDir);

    char camDir[512];
    snprintf(camDir, sizeof(camDir), "%s/FLOOR%02d/CAM%03d", folder, (int)g_currentFloor, (int)NumCamera);
    ensureMaskDirectory(camDir);

    snprintf(buf, bufSize, "%s/FLOOR%02d/CAM%03d/RID%03d_ROOM%02d_MASK%02d.png", folder, (int)g_currentFloor, (int)NumCamera, roomId, actualRoomNumber, maskId);
}

// Upscale a 320x200 R8 mask to dstW x dstH using nearest-neighbor.
// Caller must free the returned buffer.
static u8* upscaleMaskNearest(const u8* src, int dstW, int dstH)
{
    u8* dst = (u8*)malloc(dstW * dstH);
    if (!dst)
        return nullptr;
    for (int y = 0; y < dstH; y++)
    {
        int srcY = y * 200 / dstH;
        for (int x = 0; x < dstW; x++)
        {
            int srcX = x * 320 / dstW;
            dst[y * dstW + x] = src[srcY * 320 + srcX];
        }
    }
    return dst;
}

static void dumpMaskToPng(const std::array<u8, 320 * 200>& mask, int roomId, int actualRoomNumber, int maskId)
{
    static const char* dumpFolder = "masks_dump";
    ensureMaskDirectory(dumpFolder);

    char path[512];
    buildMaskFilename(path, sizeof(path), dumpFolder, roomId, actualRoomNumber, maskId);

    // Determine output resolution and prepare background data
    int outW = 320;
    int outH = 200;
    unsigned char* bgData = nullptr;
    bool needsFreeBg = false;

    // Try to get HD background data first
    if (g_currentBackgroundIsHD && g_hdBackgroundPreviewData &&
        g_hdBackgroundPreviewWidth > 0 && g_hdBackgroundPreviewHeight > 0)
    {
        outW = g_hdBackgroundPreviewWidth;
        outH = g_hdBackgroundPreviewHeight;
        bgData = g_hdBackgroundPreviewData; // Already RGBA
    }
    else
    {
        // Fallback: convert physicalScreen (paletted 320x200) to RGBA
        bgData = (unsigned char*)malloc(320 * 200 * 4);
        if (bgData)
        {
            for (int i = 0; i < 320 * 200; i++)
            {
                u8 palIdx = physicalScreen[i];
                bgData[i * 4 + 0] = RGB_Pal[palIdx * 3 + 0];
                bgData[i * 4 + 1] = RGB_Pal[palIdx * 3 + 1];
                bgData[i * 4 + 2] = RGB_Pal[palIdx * 3 + 2];
                bgData[i * 4 + 3] = 255;
            }
            needsFreeBg = true;
        }
    }

    if (!bgData)
    {
        printf(RBGFX_WARN "Failed to get background data for mask dump" CON_RESET "\n");
        return;
    }

    // Upscale mask to output resolution
    u8* upscaledMask = nullptr;
    if (outW != 320 || outH != 200)
    {
        upscaledMask = upscaleMaskNearest(mask.data(), outW, outH);
        if (!upscaledMask)
        {
            if (needsFreeBg) free(bgData);
            printf(RBGFX_WARN "Failed to upscale mask for dump" CON_RESET "\n");
            return;
        }
    }
    else
    {
        upscaledMask = (u8*)mask.data(); // No upscale needed
    }

    // Composite: background RGBA + mask overlay (semi-transparent red tint on masked pixels)
    unsigned char* compositedImage = (unsigned char*)malloc((size_t)outW * outH * 4);
    if (!compositedImage)
    {
        if (upscaledMask != mask.data()) free(upscaledMask);
        if (needsFreeBg) free(bgData);
        printf(RBGFX_WARN "Failed to allocate composited image buffer" CON_RESET "\n");
        return;
    }

    for (int i = 0; i < outW * outH; i++)
    {
        u8 maskValue = upscaledMask[i];
        // Background RGB values
        u8 bgR = bgData[i * 4 + 0];
        u8 bgG = bgData[i * 4 + 1];
        u8 bgB = bgData[i * 4 + 2];

        if (maskValue > 0)
        {
            // Mask is active: blend with semi-transparent red overlay (50% opacity)
            float maskAlpha = 0.5f;
            compositedImage[i * 4 + 0] = (u8)(bgR * (1.0f - maskAlpha) + 255 * maskAlpha);
            compositedImage[i * 4 + 1] = (u8)(bgG * (1.0f - maskAlpha));
            compositedImage[i * 4 + 2] = (u8)(bgB * (1.0f - maskAlpha));
            compositedImage[i * 4 + 3] = 255;
        }
        else
        {
            // No mask: use background as-is
            compositedImage[i * 4 + 0] = bgR;
            compositedImage[i * 4 + 1] = bgG;
            compositedImage[i * 4 + 2] = bgB;
            compositedImage[i * 4 + 3] = 255;
        }
    }

    // Write composited RGBA PNG
    stbi_write_png(path, outW, outH, 4, compositedImage, outW * 4);

    printf(RBGFX_OK "Dumped composited mask: %s (%dx%d)" CON_RESET "\n", path, outW, outH);

    // Cleanup
    free(compositedImage);
    if (upscaledMask != mask.data()) free(upscaledMask);
    if (needsFreeBg) free(bgData);
}

// Try to load a replacement mask PNG from masks_hd/.
// Returns true and fills outData / outW / outH on success.  The data is kept
// at its native resolution (HD).  Caller must free outData.
static bool loadReplacementMask(int roomId, int actualRoomNumber, int maskId, u8** outData, int* outW, int* outH)
{
    static const char* hdFolder = "masks_hd";
    char path[512];
    buildMaskFilename(path, sizeof(path), hdFolder, roomId, actualRoomNumber, maskId);

    int w, h, channels;
    unsigned char* data = stbi_load(path, &w, &h, &channels, 1); // force single channel
    if (!data)
        return false;

    *outData = data;
    *outW = w;
    *outH = h;
    return true;
}

void osystem_createMask(const std::array<u8, 320 * 200>& mask, int roomId, int maskId, int actualRoomNumber, int maskX1, int maskY1, int maskX2, int maskY2)
{
    if (maskTextures.size() < roomId + 1)
    {
        maskTextures.resize(roomId + 1);
    }
    if (maskTextures[roomId].size() < maskId + 1)
    {
        maskTextures[roomId].resize(maskId + 1);
    }

    if (bgfx::isValid(maskTextures[roomId][maskId].maskTexture))
    {
        bgfx::destroy(maskTextures[roomId][maskId].maskTexture);
        maskTextures[roomId][maskId].maskTexture = BGFX_INVALID_HANDLE;
    }

    if (bgfx::isValid(maskTextures[roomId][maskId].vertexBuffer))
    {
        bgfx::destroy(maskTextures[roomId][maskId].vertexBuffer);
        maskTextures[roomId][maskId].vertexBuffer = BGFX_INVALID_HANDLE;
    }

    // Only dump and load HD mask replacements when HD backgrounds are active
    u8* replacementData = nullptr;
    int maskW = 320;
    int maskH = 200;
    const u8* maskData = mask.data();
    if (g_currentBackgroundIsHD)
    {
        // Dump the generated mask to masks_dump/ at HD resolution for reference/editing
        // (controlled by remaster config - disabled by default for clean gameplay)
        if (g_remasterConfig.masks.dumpEnabled)
        {
            dumpMaskToPng(mask, roomId, actualRoomNumber, maskId);
        }

        // Check for a hand-edited replacement in masks_hd/ (kept at native HD resolution)
        // (controlled by remaster config - enabled by default to load artist edits)
        if (g_remasterConfig.masks.loadEnabled)
        {
            if (loadReplacementMask(roomId, actualRoomNumber, maskId, &replacementData, &maskW, &maskH))
            {
                maskData = replacementData;
            }
        }
    }

    // R8 (normalized float) with linear filtering enables bilinear AA at mask edges.
    // Clamp sampler prevents wrap-around artifacts at the texture border.
    // Texture may be HD-resolution when a replacement is loaded; the shader
    // samples via UVs so any size works.
    maskTextures[roomId][maskId].maskTexture = bgfx::createTexture2D(maskW, maskH, false, 1, bgfx::TextureFormat::R8,
        BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP,
        bgfx::copy(maskData, maskW * maskH));

    if (replacementData)
        free(replacementData);

    maskTextures[roomId][maskId].maskX1 = maskX1;
    maskTextures[roomId][maskId].maskX2 = maskX2 + 1;
    maskTextures[roomId][maskId].maskY1 = maskY1;
    maskTextures[roomId][maskId].maskY2 = maskY2 + 1;

    bgfx::VertexLayout layout;
    layout
        .begin()
        .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
        .end();

    float X1 = maskTextures[roomId][maskId].maskX1;
    float X2 = maskTextures[roomId][maskId].maskX2;
    float Y1 = maskTextures[roomId][maskId].maskY1;
    float Y2 = maskTextures[roomId][maskId].maskY2;

    float maskZ = 0.f;

    struct sVertice
    {
        float position[3];
        float texcoord[2];
    } vertexBuffer[4];

    sVertice* pVertices = vertexBuffer;
    pVertices->position[0] = X1;
    pVertices->position[1] = Y2;
    pVertices->position[2] = maskZ;
    pVertices->texcoord[0] = X1 / 320.f;
    pVertices->texcoord[1] = Y2 / 200.f;
    pVertices++;
    pVertices->position[0] = X1;
    pVertices->position[1] = Y1;
    pVertices->position[2] = maskZ;
    pVertices->texcoord[0] = X1 / 320.f;
    pVertices->texcoord[1] = Y1 / 200.f;
    pVertices++;
    pVertices->position[0] = X2;
    pVertices->position[1] = Y2;
    pVertices->position[2] = maskZ;
    pVertices->texcoord[0] = X2 / 320.f;
    pVertices->texcoord[1] = Y2 / 200.f;
    pVertices++;
    pVertices->position[0] = X2;
    pVertices->position[1] = Y1;
    pVertices->position[2] = maskZ;
    pVertices->texcoord[0] = X2 / 320.f;
    pVertices->texcoord[1] = Y1 / 200.f;
    pVertices++;

    maskTextures[roomId][maskId].vertexBuffer = bgfx::createVertexBuffer(bgfx::copy(vertexBuffer, sizeof(vertexBuffer)), layout);
}

void osystem_drawMask(int roomId, int maskId)
{
    if (g_gameId == TIMEGATE)
        return;

    if (!bgfx::isValid(maskTextures[roomId][maskId].maskTexture))
        return;

    if (!bgfx::isValid(maskTextures[roomId][maskId].vertexBuffer))
        return;

#ifdef FITD_DEBUGGER
    if (backgroundMode != backgroundModeEnum_2D)
        return;
#endif

    static bgfx::UniformHandle backgroundTextureUniform = BGFX_INVALID_HANDLE;
    if (!bgfx::isValid(backgroundTextureUniform))
    {
        backgroundTextureUniform = bgfx::createUniform("s_backgroundTexture", bgfx::UniformType::Sampler);
    }
    static bgfx::UniformHandle paletteTextureUniform = BGFX_INVALID_HANDLE;
    if (!bgfx::isValid(paletteTextureUniform))
    {
        paletteTextureUniform = bgfx::createUniform("s_paletteTexture", bgfx::UniformType::Sampler);
    }
    static bgfx::UniformHandle maskTextureUniform = BGFX_INVALID_HANDLE;
    if (!bgfx::isValid(maskTextureUniform))
    {
        maskTextureUniform = bgfx::createUniform("s_maskTexture", bgfx::UniformType::Sampler);
    }
    static bgfx::UniformHandle fadeLevelUniform = BGFX_INVALID_HANDLE;
    if (!bgfx::isValid(fadeLevelUniform))
    {
        fadeLevelUniform = bgfx::createUniform("u_fadeLevel", bgfx::UniformType::Vec4);
    }

    // Use alpha blending for HD backgrounds to get smooth mask edges
    // Write depth (far plane) so SSAO treats masked pixels as background (no occlusion)
    if (g_currentBackgroundIsHD)
    {
        bgfx::setState(0 | BGFX_STATE_WRITE_RGB
            | BGFX_STATE_WRITE_A
            | BGFX_STATE_WRITE_Z
            | BGFX_STATE_DEPTH_TEST_ALWAYS
            | BGFX_STATE_MSAA
            | BGFX_STATE_PT_TRISTRIP
            | BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_INV_SRC_ALPHA)
        );
    }
    else
    {
        bgfx::setState(0 | BGFX_STATE_WRITE_RGB
            | BGFX_STATE_WRITE_Z
            | BGFX_STATE_DEPTH_TEST_ALWAYS
            | BGFX_STATE_MSAA
            | BGFX_STATE_PT_TRISTRIP
        );
    }

    // Build transient vertex buffer with shake offset applied
    bgfx::VertexLayout maskLayout;
    maskLayout
        .begin()
        .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
        .end();

    bgfx::TransientVertexBuffer maskTransientBuffer;
    bgfx::allocTransientVertexBuffer(&maskTransientBuffer, 4, maskLayout);

    float mX1 = maskTextures[roomId][maskId].maskX1 + g_shakeOffsetX;
    float mX2 = maskTextures[roomId][maskId].maskX2 + g_shakeOffsetX;
    float mY1 = maskTextures[roomId][maskId].maskY1 + g_shakeOffsetY;
    float mY2 = maskTextures[roomId][maskId].maskY2 + g_shakeOffsetY;
    float mZ = 0.f;

    struct sMaskVertice { float position[3]; float texcoord[2]; };
    sMaskVertice* pMV = (sMaskVertice*)maskTransientBuffer.data;
    pMV[0] = { {mX1, mY2, mZ}, {maskTextures[roomId][maskId].maskX1 / 320.f, maskTextures[roomId][maskId].maskY2 / 200.f} };
    pMV[1] = { {mX1, mY1, mZ}, {maskTextures[roomId][maskId].maskX1 / 320.f, maskTextures[roomId][maskId].maskY1 / 200.f} };
    pMV[2] = { {mX2, mY2, mZ}, {maskTextures[roomId][maskId].maskX2 / 320.f, maskTextures[roomId][maskId].maskY2 / 200.f} };
    pMV[3] = { {mX2, mY1, mZ}, {maskTextures[roomId][maskId].maskX2 / 320.f, maskTextures[roomId][maskId].maskY1 / 200.f} };

    bgfx::setVertexBuffer(0, &maskTransientBuffer);

    bgfx::setTexture(2, backgroundTextureUniform, getActiveBackgroundTexture());
    bgfx::setTexture(1, paletteTextureUniform, g_paletteTexture);
    bgfx::setTexture(0, maskTextureUniform, maskTextures[roomId][maskId].maskTexture);

    // Set fade level uniform so mask matches main background brightness
    float fadeParams[4] = { g_fadeLevel, 0.0f, 0.0f, 0.0f };
    bgfx::setUniform(fadeLevelUniform, fadeParams);

    // Write stencil=1 where mask is active (fragment shader will discard transparent pixels)
    bgfx::setStencil(BGFX_STENCIL_TEST_ALWAYS
        | BGFX_STENCIL_FUNC_REF(1)
        | BGFX_STENCIL_FUNC_RMASK(0xFF)
        | BGFX_STENCIL_OP_FAIL_Z_KEEP
        | BGFX_STENCIL_OP_PASS_Z_REPLACE);

    bgfx::submit(gameViewId, g_currentBackgroundIsHD ? getHDMaskBackgroundShader() : getMaskBackgroundShader());
}

void osystem_drawMaskStencilPrepass(int roomId, int maskId)
{
    if (g_gameId == TIMEGATE)
        return;

#ifdef FITD_DEBUGGER
    if (backgroundMode != backgroundModeEnum_2D)
        return;
#endif

    if (roomId < 0 || roomId >= (int)maskTextures.size())
        return;
    if (maskId < 0 || maskId >= (int)maskTextures[roomId].size())
        return;
    if (!bgfx::isValid(maskTextures[roomId][maskId].maskTexture))
        return;
    if (!bgfx::isValid(maskTextures[roomId][maskId].vertexBuffer))
        return;

    static bgfx::UniformHandle backgroundTextureUniform = BGFX_INVALID_HANDLE;
    if (!bgfx::isValid(backgroundTextureUniform))
    {
        backgroundTextureUniform = bgfx::createUniform("s_backgroundTexture", bgfx::UniformType::Sampler);
    }
    static bgfx::UniformHandle paletteTextureUniform = BGFX_INVALID_HANDLE;
    if (!bgfx::isValid(paletteTextureUniform))
    {
        paletteTextureUniform = bgfx::createUniform("s_paletteTexture", bgfx::UniformType::Sampler);
    }
    static bgfx::UniformHandle maskTextureUniform = BGFX_INVALID_HANDLE;
    if (!bgfx::isValid(maskTextureUniform))
    {
        maskTextureUniform = bgfx::createUniform("s_maskTexture", bgfx::UniformType::Sampler);
    }
    static bgfx::UniformHandle fadeLevelUniform = BGFX_INVALID_HANDLE;
    if (!bgfx::isValid(fadeLevelUniform))
    {
        fadeLevelUniform = bgfx::createUniform("u_fadeLevel", bgfx::UniformType::Vec4);
    }

    // Stencil-only pass: no color or depth writes.
    // The mask shader discards transparent pixels, so stencil is only
    // written where the mask foreground is opaque.
    bgfx::setState(0
        | BGFX_STATE_MSAA
        | BGFX_STATE_PT_TRISTRIP
    );

    bgfx::VertexLayout maskLayout;
    maskLayout
        .begin()
        .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
        .end();

    bgfx::TransientVertexBuffer maskTransientBuffer;
    bgfx::allocTransientVertexBuffer(&maskTransientBuffer, 4, maskLayout);

    float mX1 = maskTextures[roomId][maskId].maskX1 + g_shakeOffsetX;
    float mX2 = maskTextures[roomId][maskId].maskX2 + g_shakeOffsetX;
    float mY1 = maskTextures[roomId][maskId].maskY1 + g_shakeOffsetY;
    float mY2 = maskTextures[roomId][maskId].maskY2 + g_shakeOffsetY;
    float mZ = 0.f;

    struct sMaskVertice { float position[3]; float texcoord[2]; };
    sMaskVertice* pMV = (sMaskVertice*)maskTransientBuffer.data;
    pMV[0] = { {mX1, mY2, mZ}, {maskTextures[roomId][maskId].maskX1 / 320.f, maskTextures[roomId][maskId].maskY2 / 200.f} };
    pMV[1] = { {mX1, mY1, mZ}, {maskTextures[roomId][maskId].maskX1 / 320.f, maskTextures[roomId][maskId].maskY1 / 200.f} };
    pMV[2] = { {mX2, mY2, mZ}, {maskTextures[roomId][maskId].maskX2 / 320.f, maskTextures[roomId][maskId].maskY2 / 200.f} };
    pMV[3] = { {mX2, mY1, mZ}, {maskTextures[roomId][maskId].maskX2 / 320.f, maskTextures[roomId][maskId].maskY1 / 200.f} };

    bgfx::setVertexBuffer(0, &maskTransientBuffer);

    bgfx::setTexture(2, backgroundTextureUniform, getActiveBackgroundTexture());
    bgfx::setTexture(1, paletteTextureUniform, g_paletteTexture);
    bgfx::setTexture(0, maskTextureUniform, maskTextures[roomId][maskId].maskTexture);

    float fadeParams[4] = { g_fadeLevel, 0.0f, 0.0f, 0.0f };
    bgfx::setUniform(fadeLevelUniform, fadeParams);

    // Write stencil=2 where mask foreground is opaque, so shadows
    // (which test stencil==0) are rejected at these pixels.
    bgfx::setStencil(BGFX_STENCIL_TEST_ALWAYS
        | BGFX_STENCIL_FUNC_REF(2)
        | BGFX_STENCIL_FUNC_RMASK(0xFF)
        | BGFX_STENCIL_OP_FAIL_Z_KEEP
        | BGFX_STENCIL_OP_PASS_Z_REPLACE);

    bgfx::submit(gameViewId, g_currentBackgroundIsHD ? getHDMaskBackgroundShader() : getMaskBackgroundShader());
}

///////////////////////////////////////////////////////////////////////////////
// Scene preview capture for pause menu
// A persistent GPU snapshot texture is blitted every game frame (inside
// EndFrame, after endScene, before bgfx::frame).  When the pause menu opens
// we stop updating the snapshot, request a CPU readback of the last game
// frame, and use that RGBA data as the menu preview thumbnail.
///////////////////////////////////////////////////////////////////////////////

static const bgfx::ViewId kViewSnapshot = 206;

static bgfx::TextureHandle s_snapshotTex = BGFX_INVALID_HANDLE;
static unsigned char* s_previewData = nullptr;
static int s_snapshotWidth = 0;
static int s_snapshotHeight = 0;
static bool s_snapshotEnabled = true;   // true during gameplay, false while menu is open
static bool s_previewReady = false;

// Sampleable texture for rendering the frozen scene as a background during menus
// Called every frame from EndFrame() (after endScene, before bgfx::frame).
// Blits the composited scene from m_mainColorTex into a persistent snapshot
// texture so it survives the clear at the start of the next frame.
void osystem_updateSceneSnapshot()
{
    if (!s_snapshotEnabled)
        return;

    if (!g_postProcessing || !bgfx::isValid(g_postProcessing->getMainColorTexture()))
        return;

    int w = g_postProcessing->getWidth();
    int h = g_postProcessing->getHeight();
    if (w <= 0 || h <= 0)
        return;

    // Recreate snapshot texture if resolution changed
    if (w != s_snapshotWidth || h != s_snapshotHeight)
    {
        if (bgfx::isValid(s_snapshotTex))
            bgfx::destroy(s_snapshotTex);
        if (s_previewData)
        {
            delete[] s_previewData;
            s_previewData = nullptr;
        }

        s_snapshotTex = bgfx::createTexture2D(
            (uint16_t)w, (uint16_t)h, false, 1,
            bgfx::TextureFormat::RGBA8,
            BGFX_TEXTURE_BLIT_DST | BGFX_TEXTURE_READ_BACK
        );

        s_previewData = new unsigned char[(size_t)w * h * 4];
        s_snapshotWidth = w;
        s_snapshotHeight = h;
    }

    // Recreate sampleable frozen scene texture if resolution changed
    if (w != s_frozenSceneWidth || h != s_frozenSceneHeight)
    {
        if (bgfx::isValid(s_frozenSceneTex))
            bgfx::destroy(s_frozenSceneTex);
        s_frozenSceneTex = bgfx::createTexture2D(
            (uint16_t)w, (uint16_t)h, false, 1,
            bgfx::TextureFormat::RGBA8,
            BGFX_TEXTURE_BLIT_DST | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP
        );
        s_frozenSceneWidth = w;
        s_frozenSceneHeight = h;
    }

    // Set up snapshot view (after composite view 205) so the blit happens
    // after the post-processing chain has written the final image.
    bgfx::setViewName(kViewSnapshot, "Snapshot");
    bgfx::setViewRect(kViewSnapshot, 0, 0, (uint16_t)w, (uint16_t)h);
    bgfx::touch(kViewSnapshot);

    // GPU blit: m_mainColorTex -> s_snapshotTex (for CPU readback)
    bgfx::blit(kViewSnapshot, s_snapshotTex, 0, 0, g_postProcessing->getMainColorTexture());

    // GPU blit: m_mainColorTex -> s_frozenSceneTex (sampleable, for menu background)
    if (bgfx::isValid(s_frozenSceneTex))
        bgfx::blit(kViewSnapshot, s_frozenSceneTex, 0, 0, g_postProcessing->getMainColorTexture());
}

// Called when the pause menu opens.  Freezes snapshot updates and kicks off
// a CPU readback of the last captured frame.
void osystem_captureScenePreview()
{
    s_previewReady = false;
    s_snapshotEnabled = false;  // freeze   keep the last game frame in the snapshot

    if (!bgfx::isValid(s_snapshotTex) || !s_previewData || s_snapshotWidth <= 0 || s_snapshotHeight <= 0)
        return;

    // Request async readback   data is available after 2 bgfx::frame() calls.
    bgfx::readTexture(s_snapshotTex, s_previewData);
}

// Call after pumping 2 bgfx::frame() calls so the readback has completed.
void osystem_finalizeScenePreview()
{
    if (s_previewData && s_snapshotWidth > 0 && s_snapshotHeight > 0)
    {
        s_previewReady = true;
    }
}

// Called when the pause menu closes.  Re-enables per-frame snapshot updates.
void osystem_releaseScenePreview()
{
    s_snapshotEnabled = true;
    s_previewReady = false;
}

unsigned char* osystem_getScenePreviewData()
{
    return s_previewReady ? s_previewData : nullptr;
}

int osystem_getScenePreviewWidth()
{
    return s_previewReady ? s_snapshotWidth : 0;
}

int osystem_getScenePreviewHeight()
{
    return s_previewReady ? s_snapshotHeight : 0;
}

// Freeze/unfreeze the scene snapshot for menu backgrounds
void osystem_freezeSceneForMenu()
{
    s_snapshotEnabled = false;
}

void osystem_unfreezeSceneForMenu()
{
    s_snapshotEnabled = true;
}

// Render the frozen scene snapshot as a full-screen background at Z=1000.
// Used during inventory/found-object menus to preserve 3D objects in the scene.
void osystem_drawFrozenSceneBackground()
{
    if (backgroundMode != backgroundModeEnum_2D)
        return;

    if (!bgfx::isValid(s_frozenSceneTex))
        return;

    bgfx::VertexLayout layout;
    layout
        .begin()
        .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
        .end();

    bgfx::TransientVertexBuffer transientBuffer;
    bgfx::allocTransientVertexBuffer(&transientBuffer, 6, layout);

    struct sVertice
    {
        float position[3];
        float texcoord[2];
    };

    sVertice* pVertices = (sVertice*)transientBuffer.data;

    // Full-screen quad at Z=1000 (same depth as normal background)
    pVertices->position[0] = 0.f;   pVertices->position[1] = 0.f;   pVertices->position[2] = 1000.f;
    pVertices->texcoord[0] = 0.f;   pVertices->texcoord[1] = 0.f;
    pVertices++;

    pVertices->position[0] = 320.f; pVertices->position[1] = 200.f; pVertices->position[2] = 1000.f;
    pVertices->texcoord[0] = 1.f;   pVertices->texcoord[1] = 1.f;
    pVertices++;

    pVertices->position[0] = 320.f; pVertices->position[1] = 0.f;   pVertices->position[2] = 1000.f;
    pVertices->texcoord[0] = 1.f;   pVertices->texcoord[1] = 0.f;
    pVertices++;

    pVertices->position[0] = 0.f;   pVertices->position[1] = 0.f;   pVertices->position[2] = 1000.f;
    pVertices->texcoord[0] = 0.f;   pVertices->texcoord[1] = 0.f;
    pVertices++;

    pVertices->position[0] = 0.f;   pVertices->position[1] = 200.f; pVertices->position[2] = 1000.f;
    pVertices->texcoord[0] = 0.f;   pVertices->texcoord[1] = 1.f;
    pVertices++;

    pVertices->position[0] = 320.f; pVertices->position[1] = 200.f; pVertices->position[2] = 1000.f;
    pVertices->texcoord[0] = 1.f;   pVertices->texcoord[1] = 1.f;
    pVertices++;

    static bgfx::UniformHandle frozenSceneTexUniform = BGFX_INVALID_HANDLE;
    if (!bgfx::isValid(frozenSceneTexUniform))
    {
        frozenSceneTexUniform = bgfx::createUniform("s_backgroundTexture", bgfx::UniformType::Sampler);
    }
    static bgfx::UniformHandle frozenSceneFadeUniform = BGFX_INVALID_HANDLE;
    if (!bgfx::isValid(frozenSceneFadeUniform))
    {
        frozenSceneFadeUniform = bgfx::createUniform("u_fadeLevel", bgfx::UniformType::Vec4);
    }

    bgfx::setState(0 | BGFX_STATE_WRITE_RGB
        | BGFX_STATE_MSAA
    );

    bgfx::setVertexBuffer(0, &transientBuffer);
    bgfx::setTexture(0, frozenSceneTexUniform, s_frozenSceneTex);

    float fadeParams[4] = { g_fadeLevel, 0.0f, 0.0f, 0.0f };
    bgfx::setUniform(frozenSceneFadeUniform, fadeParams);

    bgfx::submit(gameViewId, getHDBackgroundShader());
}

// Page turn animation support
static unsigned char s_pageTurnOldPage[320 * 200];

void osystem_pageTurnCapture()
{
    memcpy(s_pageTurnOldPage, physicalScreen, 320 * 200);
}

void osystem_pageTurnFrame(float progress, bool forward, unsigned char* newPage)
{
    unsigned char compositedPage[320 * 200];

    // Ease-out curve for smooth deceleration
    float easedProgress = 1.0f - (1.0f - progress) * (1.0f - progress);
    int offset = (int)(320.0f * easedProgress);

    if (offset <= 0)
    {
        bgfx::updateTexture2D(g_backgroundTexture, 0, 0, 0, 0, 320, 200,
            bgfx::copy(s_pageTurnOldPage, 320 * 200));
        return;
    }
    if (offset >= 320)
    {
        bgfx::updateTexture2D(g_backgroundTexture, 0, 0, 0, 0, 320, 200,
            bgfx::copy(newPage, 320 * 200));
        return;
    }

    for (int y = 0; y < 200; y++)
    {
        for (int x = 0; x < 320; x++)
        {
            if (forward)
            {
                int srcX = x + offset;
                if (srcX < 320)
                    compositedPage[y * 320 + x] = s_pageTurnOldPage[y * 320 + srcX];
                else
                    compositedPage[y * 320 + x] = newPage[y * 320 + (srcX - 320)];
            }
            else
            {
                int srcX = x - offset;
                if (srcX >= 0)
                    compositedPage[y * 320 + x] = s_pageTurnOldPage[y * 320 + srcX];
                else
                    compositedPage[y * 320 + x] = newPage[y * 320 + (320 + srcX)];
            }
        }
    }

    // Draw a 2-pixel black dividing line at the fold edge
    int splitX = forward ? (320 - offset) : offset;
    if (splitX > 0 && splitX < 319)
    {
        for (int y = 0; y < 200; y++)
        {
            compositedPage[y * 320 + splitX] = 0;
            compositedPage[y * 320 + splitX + 1] = 0;
        }
    }

    bgfx::updateTexture2D(g_backgroundTexture, 0, 0, 0, 0, 320, 200,
        bgfx::copy(compositedPage, 320 * 200));
}

// Map screen texture overlay
static bgfx::TextureHandle g_mapTexture = BGFX_INVALID_HANDLE;
static int g_mapLoadedFloor = -1;

void osystem_loadMapTexture(int floorNumber)
{
    // Already loaded for this floor
    if (g_mapLoadedFloor == floorNumber && bgfx::isValid(g_mapTexture))
        return;

    // Destroy previous texture if any
    if (bgfx::isValid(g_mapTexture))
    {
        bgfx::destroy(g_mapTexture);
        g_mapTexture = BGFX_INVALID_HANDLE;
    }

    char filename[64];
    snprintf(filename, sizeof(filename), "ETAGE%02d.png", floorNumber);

    printf(RBGFX_TAG "Loading map texture: %s (floor %d)\n", filename, floorNumber);

    int width, height, channels;
    unsigned char* data = loadHDImageFile(filename, &width, &height, &channels);
    if (!data)
    {
        printf(RBGFX_WARN "Map texture not found: %s" CON_RESET "\n", filename);
        g_mapLoadedFloor = -1;
        return;
    }

    unsigned char* rgbaData = data;
    bool needsFree = false;
    if (channels == 3)
    {
        rgbaData = (unsigned char*)malloc(width * height * 4);
        if (rgbaData)
        {
            for (int p = 0; p < width * height; p++)
            {
                rgbaData[p * 4 + 0] = data[p * 3 + 0];
                rgbaData[p * 4 + 1] = data[p * 3 + 1];
                rgbaData[p * 4 + 2] = data[p * 3 + 2];
                rgbaData[p * 4 + 3] = 255;
            }
            needsFree = true;
        }
    }

    if (rgbaData)
    {
        g_mapTexture = bgfx::createTexture2D(width, height, false, 1,
            bgfx::TextureFormat::RGBA8, BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP,
            bgfx::copy(rgbaData, width * height * 4));
    }

    if (needsFree)
        free(rgbaData);
    freeHDImageData(data);

    printf(RBGFX_OK "Map texture loaded: %s (%dx%d, %d channels)\n", filename, width, height, channels);
    g_mapLoadedFloor = floorNumber;
}

void osystem_drawMapImage()
{
    if (backgroundMode != backgroundModeEnum_2D)
        return;

    if (!bgfx::isValid(g_mapTexture))
        return;

    bgfx::VertexLayout layout;
    layout
        .begin()
        .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
        .end();

    bgfx::TransientVertexBuffer transientBuffer;
    bgfx::allocTransientVertexBuffer(&transientBuffer, 6, layout);

    struct sVertice
    {
        float position[3];
        float texcoord[2];
    };

    sVertice* pVertices = (sVertice*)transientBuffer.data;

    pVertices->position[0] = 0.f;   pVertices->position[1] = 0.f;   pVertices->position[2] = 999.f;
    pVertices->texcoord[0] = 0.f;   pVertices->texcoord[1] = 0.f;
    pVertices++;

    pVertices->position[0] = 320.f; pVertices->position[1] = 200.f; pVertices->position[2] = 999.f;
    pVertices->texcoord[0] = 1.f;   pVertices->texcoord[1] = 1.f;
    pVertices++;

    pVertices->position[0] = 320.f; pVertices->position[1] = 0.f;   pVertices->position[2] = 999.f;
    pVertices->texcoord[0] = 1.f;   pVertices->texcoord[1] = 0.f;
    pVertices++;

    pVertices->position[0] = 0.f;   pVertices->position[1] = 0.f;   pVertices->position[2] = 999.f;
    pVertices->texcoord[0] = 0.f;   pVertices->texcoord[1] = 0.f;
    pVertices++;

    pVertices->position[0] = 0.f;   pVertices->position[1] = 200.f; pVertices->position[2] = 999.f;
    pVertices->texcoord[0] = 0.f;   pVertices->texcoord[1] = 1.f;
    pVertices++;

    pVertices->position[0] = 320.f; pVertices->position[1] = 200.f; pVertices->position[2] = 999.f;
    pVertices->texcoord[0] = 1.f;   pVertices->texcoord[1] = 1.f;
    pVertices++;

    static bgfx::UniformHandle mapTexUniform = BGFX_INVALID_HANDLE;
    if (!bgfx::isValid(mapTexUniform))
    {
        mapTexUniform = bgfx::createUniform("s_backgroundTexture", bgfx::UniformType::Sampler);
    }
    static bgfx::UniformHandle mapFadeUniform = BGFX_INVALID_HANDLE;
    if (!bgfx::isValid(mapFadeUniform))
    {
        mapFadeUniform = bgfx::createUniform("u_fadeLevel", bgfx::UniformType::Vec4);
    }

    bgfx::setState(0 | BGFX_STATE_WRITE_RGB
        | BGFX_STATE_WRITE_A
        | BGFX_STATE_MSAA
        | BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_INV_SRC_ALPHA)
    );

    bgfx::setVertexBuffer(0, &transientBuffer);
    bgfx::setTexture(0, mapTexUniform, g_mapTexture);

    float fadeParams[4] = { g_fadeLevel, 0.0f, 0.0f, 0.0f };
    bgfx::setUniform(mapFadeUniform, fadeParams);

    bgfx::submit(gameViewId, getHDBackgroundShader());
}

void osystem_destroyMapTexture()
{
    if (bgfx::isValid(g_mapTexture))
    {
        bgfx::destroy(g_mapTexture);
        g_mapTexture = BGFX_INVALID_HANDLE;
    }
    g_mapLoadedFloor = -1;
}



