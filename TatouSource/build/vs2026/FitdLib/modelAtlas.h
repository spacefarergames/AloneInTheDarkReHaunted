///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Copyright (C) 2026 Infogrames / Spacefarer Retro Remasters LLC
// Based on FITD by yaz0r, Re-haunted is released under GPL
// Author: Jake Jackson (jake@spacefarergames.com)
//
// Model texture atlas system - generates UV-mapped texture atlases from
// flat-shaded model polygons for artist-paintable texture overrides
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include "common.h"
#include <bgfx/bgfx.h>
#include <vector>
#include <unordered_map>
#include <string>

struct sBody;

// Per-vertex UV coordinates for a single polygon in the atlas
struct AtlasPolyUVs
{
    std::vector<float> u; // one per vertex
    std::vector<float> v; // one per vertex
};

// UV coordinates for a sphere primitive in the atlas (billboard quad mapping)
struct AtlasSphereUVs
{
    float centerU = 0.f;  // center of sphere in atlas UV space
    float centerV = 0.f;
    float radiusU = 0.f;  // radius in UV space (for billboard sizing)
    float radiusV = 0.f;
    bool valid = false;   // true if this sphere has atlas data
};

// Atlas data associated with a model body
struct ModelAtlasData
{
    // Polygon atlas (orthographic projection)
    bgfx::TextureHandle texture = BGFX_INVALID_HANDLE;
    int atlasWidth = 0;
    int atlasHeight = 0;
    std::vector<AtlasPolyUVs> polyUVs;     // indexed by primitive index

    // Sphere atlas (separate texture, one cell per sphere)
    bgfx::TextureHandle sphereTexture = BGFX_INVALID_HANDLE;
    int sphereAtlasWidth = 0;
    int sphereAtlasHeight = 0;
    std::vector<AtlasSphereUVs> sphereUVs; // indexed by primitive index
};

// Dump a flat-shaded texture atlas PNG for the given body.
// The output file is written to <homePath>/atlases/body_<bodyNum>.png
// Returns true on success.
bool dumpModelAtlas(int bodyNum, sBody* pBody);

// Try to load a texture atlas for the given body.
// Looks for <homePath>/atlases/body_<bodyNum>.png
// If found, creates a bgfx texture and computes UVs, stores in cache.
// Returns pointer to cached atlas data, or nullptr if no atlas file exists.
ModelAtlasData* loadModelAtlas(int bodyNum, sBody* pBody);

// Get cached atlas without loading (returns nullptr if not cached)
ModelAtlasData* getModelAtlas(int bodyNum);

// Clear all cached atlas data (call on shutdown).
void clearModelAtlases();
