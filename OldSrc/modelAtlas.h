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

// Atlas data associated with a model body
struct ModelAtlasData
{
    bgfx::TextureHandle texture = BGFX_INVALID_HANDLE;
    int atlasWidth = 0;
    int atlasHeight = 0;
    std::vector<AtlasPolyUVs> polyUVs; // indexed by primitive index
    std::vector<unsigned char> pixels; // CPU-side RGBA copy for transparency sampling

    // Separate texture for sphere/billboard primitives
    bgfx::TextureHandle sphereTexture = BGFX_INVALID_HANDLE;
    int sphereAtlasWidth = 0;
    int sphereAtlasHeight = 0;

    // Separate texture for flat-shaded polygons on mixed (textured+flat) models
    // This allows texturing elements like door handles separately from the main door texture
    bgfx::TextureHandle flatTexture = BGFX_INVALID_HANDLE;
    int flatAtlasWidth = 0;
    int flatAtlasHeight = 0;
    std::vector<AtlasPolyUVs> flatPolyUVs; // indexed by primitive index, only for primTypeEnum_Poly
    std::vector<unsigned char> flatPixels; // CPU-side RGBA copy for transparency sampling

    // Separate texture for ramp-shaded polygons (material 3-6: marbre, copper, copper2, marbre2)
    bgfx::TextureHandle rampTexture = BGFX_INVALID_HANDLE;
    int rampAtlasWidth = 0;
    int rampAtlasHeight = 0;
    std::vector<AtlasPolyUVs> rampPolyUVs; // indexed by primitive index, only for ramp material polys
    std::vector<unsigned char> rampPixels; // CPU-side RGBA copy for transparency sampling

    // Separate texture for other material polygons (material 1: dither, material 2: transparent)
    bgfx::TextureHandle otherTexture = BGFX_INVALID_HANDLE;
    int otherAtlasWidth = 0;
    int otherAtlasHeight = 0;
    std::vector<AtlasPolyUVs> otherPolyUVs; // indexed by primitive index, only for material 1-2 polys
    std::vector<unsigned char> otherPixels; // CPU-side RGBA copy for transparency sampling
};

// Dump a flat-shaded texture atlas PNG for the given body.
// The output file is written to <homePath>/atlases/body_<hqrName>_<bodyNum>.png
// Returns true on success.
bool dumpModelAtlas(int bodyNum, sBody* pBody, const std::string& hqrName);

// Dump sphere/billboard textures to a separate texture atlas.
// The output file is written to <homePath>/atlases/spheres_<hqrName>_<bodyNum>.png
// Returns true on success.
bool dumpSphereAtlas(int bodyNum, sBody* pBody, const std::string& hqrName);

// Dump flat-shaded polygons from mixed (textured+flat) models to a separate atlas.
// This allows texturing elements like door handles/panels separately.
// The output file is written to <homePath>/atlases/flat_<hqrName>_<bodyNum>.png
// Returns true on success.
bool dumpFlatPolyAtlas(int bodyNum, sBody* pBody, const std::string& hqrName);

// Dump ramp-shaded polygons (material 3-6: marbre, copper, copper2, marbre2) to a separate atlas.
// The output file is written to <homePath>/atlases/ramp_<hqrName>_<bodyNum>.png
// Returns true on success.
bool dumpRampAtlas(int bodyNum, sBody* pBody, const std::string& hqrName);

// Dump other material polygons (material 1: dither, material 2: transparent) to a separate atlas.
// The output file is written to <homePath>/atlases/other_<hqrName>_<bodyNum>.png
// Returns true on success.
bool dumpOtherAtlas(int bodyNum, sBody* pBody, const std::string& hqrName);

// Try to load a texture atlas for the given body.
// Looks for <homePath>/atlases/body_<hqrName>_<bodyNum>.png
// If found, creates a bgfx texture and computes UVs, stores in cache.
// Returns pointer to cached atlas data, or nullptr if no atlas file exists.
ModelAtlasData* loadModelAtlas(int bodyNum, sBody* pBody, const std::string& hqrName);

// Get a previously loaded atlas (does not trigger load/dump).
ModelAtlasData* getModelAtlas(int bodyNum);

// Clear all cached atlas data (call on shutdown).
void clearModelAtlases();
