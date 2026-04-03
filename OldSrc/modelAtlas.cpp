///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Copyright (C) 2026 Infogrames / Spacefarer Retro Remasters LLC
// Based on FITD by yaz0r, Re-haunted is released under GPL
// Author: Jake Jackson (jake@spacefarergames.com)
//
// Model texture atlas generation, UV computation, and runtime loading
///////////////////////////////////////////////////////////////////////////////

#include "common.h"
#include "modelAtlas.h"
#include "consoleLog.h"
#include "hdArchive.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <algorithm>
#include <string>

#define STB_IMAGE_WRITE_STATIC
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../ThirdParty/bgfx.cmake/bimg/3rdparty/stb/stb_image_write.h"

#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#include "../ThirdParty/bgfx.cmake/bimg/3rdparty/stb/stb_image.h"

extern "C" {
    extern char homePath[512];
}

extern char RGB_Pal[256 * 3];

// Cache of loaded atlases keyed by HQR source name + body number
static std::unordered_map<std::string, ModelAtlasData> s_atlasCache;

static std::string makeAtlasKey(const std::string& hqrName, int bodyNum)
{
    return hqrName + "_" + std::to_string(bodyNum);
}

// Padding between polygons in the atlas (pixels)
static const int ATLAS_PADDING = 2;
// Minimum cell size for each polygon in the atlas
static const int ATLAS_CELL_MIN = 16;
// Max cols per group row in the atlas
static const int ATLAS_GROUP_MAX_COLS = 8;

// Determine which group (bone/body part) a primitive belongs to
// by finding which group's vertex range contains the primitive's first vertex
static int getGroupForPrimitive(sBody* pBody, sPrimitive* pPrim)
{
    if (pPrim->m_points.empty()) return 0;
    int vertIdx = pPrim->m_points[0];
    for (int g = 0; g < (int)pBody->m_groups.size(); g++)
    {
        sGroup* pGroup = &pBody->m_groups[g];
        if (vertIdx >= pGroup->m_start && vertIdx < pGroup->m_start + pGroup->m_numVertices)
            return g;
    }
    return 0;
}

// Shared atlas layout data — computed identically by both dump and load
struct AtlasCellInfo
{
    int primIdx;   // original primitive index
    int group;     // body part group
    int w, h;      // cell pixel size
    int x, y;      // cell position in atlas
};

struct AtlasLayout
{
    std::vector<AtlasCellInfo> cells;
    int atlasW, atlasH;
};

static bool isPrimPoly(sPrimitive* pPrim)
{
    return pPrim->m_type == primTypeEnum_Poly || pPrim->m_type == processPrim_PolyTexture9 || pPrim->m_type == processPrim_PolyTexture10;
}

static bool isPrimSphere(sPrimitive* pPrim)
{
    return pPrim->m_type == primTypeEnum_Sphere;
}

static bool isPrimRamp(sPrimitive* pPrim)
{
    // Ramp-shaded polygons: material 3 (marbre), 4 (copper), 5 (copper2), 6 (marbre2)
    return isPrimPoly(pPrim) && pPrim->m_material >= 3 && pPrim->m_material <= 6;
}

static bool isPrimOther(sPrimitive* pPrim)
{
    // Other material polygons: material 1 (dither), 2 (transparent)
    return isPrimPoly(pPrim) && (pPrim->m_material == 1 || pPrim->m_material == 2);
}

// Forward declarations for functions defined later in this file
static void computePolyFlatSize(sBody* pBody, sPrimitive* pPrim, float& outW, float& outH);
static void projectPolyToUV(sBody* pBody, sPrimitive* pPrim, std::vector<float>& outU, std::vector<float>& outV);

// Compute rest-pose global vertex positions by applying bone hierarchy offsets.
static void computeRestPoseVertices(sBody* pBody, std::vector<point3dStruct>& outVerts)
{
    int numVerts = (int)pBody->m_vertices.size();
    outVerts.resize(numVerts);
    for (int i = 0; i < numVerts; i++)
        outVerts[i] = pBody->m_vertices[i];

    for (int i = 0; i < (int)pBody->m_groups.size(); i++)
    {
        sGroup* pGroup = &pBody->m_groups[i];
        for (int j = 0; j < pGroup->m_numVertices; j++)
        {
            outVerts[pGroup->m_start + j].x += outVerts[pGroup->m_baseVertices].x;
            outVerts[pGroup->m_start + j].y += outVerts[pGroup->m_baseVertices].y;
            outVerts[pGroup->m_start + j].z += outVerts[pGroup->m_baseVertices].z;
        }
    }
}

// Check if a polygon faces the front (toward -Z) using rest-pose global vertices
static bool isPolyFrontFacing(const std::vector<point3dStruct>& globalVerts, sPrimitive* pPrim)
{
    int nv = (int)pPrim->m_points.size();
    if (nv < 3) return true;
    const point3dStruct& v0 = globalVerts[pPrim->m_points[0]];
    const point3dStruct& v1 = globalVerts[pPrim->m_points[1]];
    const point3dStruct& v2 = globalVerts[pPrim->m_points[2]];
    float e1x = (float)(v1.x - v0.x), e1y = (float)(v1.y - v0.y), e1z = (float)(v1.z - v0.z);
    float e2x = (float)(v2.x - v0.x), e2y = (float)(v2.y - v0.y), e2z = (float)(v2.z - v0.z);
    float nz = e1x * e2y - e1y * e2x;
    return nz < 0;
}

// Detect if polygon faces left (negative X direction)
static bool isPolyLeftFacing(const std::vector<point3dStruct>& globalVerts, sPrimitive* pPrim)
{
    int nv = (int)pPrim->m_points.size();
    if (nv < 3) return false;
    const point3dStruct& v0 = globalVerts[pPrim->m_points[0]];
    const point3dStruct& v1 = globalVerts[pPrim->m_points[1]];
    const point3dStruct& v2 = globalVerts[pPrim->m_points[2]];
    float e1x = (float)(v1.x - v0.x), e1y = (float)(v1.y - v0.y), e1z = (float)(v1.z - v0.z);
    float e2x = (float)(v2.x - v0.x), e2y = (float)(v2.y - v0.y), e2z = (float)(v2.z - v0.z);
    // Cross product: e1 x e2 = (ny*e2z - nz*e2y, nz*e2x - nx*e2z, nx*e2y - ny*e2x)
    // nx = e1y * e2z - e1z * e2y
    float nx = e1y * e2z - e1z * e2y;
    return nx < 0; // Negative X component means left-facing
}

// Detect if polygon faces right (positive X direction)
static bool isPolyRightFacing(const std::vector<point3dStruct>& globalVerts, sPrimitive* pPrim)
{
    int nv = (int)pPrim->m_points.size();
    if (nv < 3) return false;
    const point3dStruct& v0 = globalVerts[pPrim->m_points[0]];
    const point3dStruct& v1 = globalVerts[pPrim->m_points[1]];
    const point3dStruct& v2 = globalVerts[pPrim->m_points[2]];
    float e1x = (float)(v1.x - v0.x), e1y = (float)(v1.y - v0.y), e1z = (float)(v1.z - v0.z);
    float e2x = (float)(v2.x - v0.x), e2y = (float)(v2.y - v0.y), e2z = (float)(v2.z - v0.z);
    float nx = e1y * e2z - e1z * e2y;
    return nx > 0; // Positive X component means right-facing
}

struct ProjectionParams
{
    float padMinX, padMinY;
    float rangeX, rangeY;
    int atlasW, atlasH;
};

static ProjectionParams computeProjectionParams(const std::vector<point3dStruct>& verts)
{
    ProjectionParams p = {};
    float minX = 1e30f, maxX = -1e30f, minY = 1e30f, maxY = -1e30f;
    for (auto& v : verts)
    {
        float fx = (float)v.x, fy = (float)v.y;
        if (fx < minX) minX = fx;
        if (fx > maxX) maxX = fx;
        if (fy < minY) minY = fy;
        if (fy > maxY) maxY = fy;
    }
    float rawRangeX = maxX - minX;
    float rawRangeY = maxY - minY;
    if (rawRangeX < 1.f) rawRangeX = 1.f;
    if (rawRangeY < 1.f) rawRangeY = 1.f;
    float padX = rawRangeX * 0.05f;
    float padY = rawRangeY * 0.05f;
    p.padMinX = minX - padX;
    p.padMinY = minY - padY;
    p.rangeX = rawRangeX + 2.f * padX;
    p.rangeY = rawRangeY + 2.f * padY;
    auto nextPow2 = [](int v) -> int {
        v--; v |= v >> 1; v |= v >> 2; v |= v >> 4; v |= v >> 8; v |= v >> 16;
        return v + 1;
    };
    int halfW = 512;
    int baseH = (int)ceilf((float)halfW * (p.rangeY / p.rangeX));
    p.atlasW = nextPow2((std::max)(halfW * 2, 128));
    p.atlasH = nextPow2((std::max)(baseH, 64));
    return p;
}

static void fillTriangleInImage(
    std::vector<unsigned char>& image, int imgW, int imgH,
    float x0, float y0, float x1, float y1, float x2, float y2,
    unsigned char r, unsigned char g, unsigned char b)
{
    if (y0 > y1) { std::swap(x0, x1); std::swap(y0, y1); }
    if (y0 > y2) { std::swap(x0, x2); std::swap(y0, y2); }
    if (y1 > y2) { std::swap(x1, x2); std::swap(y1, y2); }
    if (y2 - y0 < 0.001f) return;
    int yStart = (std::max)(0, (int)ceilf(y0));
    int yEnd = (std::min)(imgH - 1, (int)floorf(y2));
    for (int y = yStart; y <= yEnd; y++)
    {
        float fy = (float)y + 0.5f;
        float t02 = (fy - y0) / (y2 - y0);
        float xLong = x0 + t02 * (x2 - x0);
        float xShort;
        if (fy < y1)
        {
            float dy01 = y1 - y0;
            xShort = (dy01 > 0.001f) ? x0 + (fy - y0) / dy01 * (x1 - x0) : x0;
        }
        else
        {
            float dy12 = y2 - y1;
            xShort = (dy12 > 0.001f) ? x1 + (fy - y1) / dy12 * (x2 - x1) : x1;
        }
        float xLeft = fminf(xLong, xShort);
        float xRight = fmaxf(xLong, xShort);
        int xS = (std::max)(0, (int)ceilf(xLeft));
        int xE = (std::min)(imgW - 1, (int)floorf(xRight));
        for (int x = xS; x <= xE; x++)
        {
            int offset = (y * imgW + x) * 4;
            image[offset + 0] = r;
            image[offset + 1] = g;
            image[offset + 2] = b;
            image[offset + 3] = 255;
        }
    }
}

static AtlasLayout computeAtlasLayout(sBody* pBody)
{
    AtlasLayout layout;
    int numPrims = (int)pBody->m_primitives.size();

    // Build cell info with group assignment
    layout.cells.resize(numPrims);
    float maxDim = 0;
    for (int i = 0; i < numPrims; i++)
    {
        sPrimitive* pPrim = &pBody->m_primitives[i];
        layout.cells[i].primIdx = i;
        layout.cells[i].group = getGroupForPrimitive(pBody, pPrim);

        if (!isPrimPoly(pPrim))
        {
            layout.cells[i].w = ATLAS_CELL_MIN;
            layout.cells[i].h = ATLAS_CELL_MIN;
            continue;
        }
        float w, h;
        computePolyFlatSize(pBody, pPrim, w, h);
        if (w > maxDim) maxDim = w;
        if (h > maxDim) maxDim = h;
        layout.cells[i].w = (std::max)(ATLAS_CELL_MIN, (int)ceilf(w));
        layout.cells[i].h = (std::max)(ATLAS_CELL_MIN, (int)ceilf(h));
    }

    // Normalize cell sizes so largest face maps to ~64px
    float scale = (maxDim > 0) ? 64.f / maxDim : 1.f;
    for (int i = 0; i < numPrims; i++)
    {
        layout.cells[i].w = (std::max)(ATLAS_CELL_MIN, (int)ceilf(layout.cells[i].w * scale));
        layout.cells[i].h = (std::max)(ATLAS_CELL_MIN, (int)ceilf(layout.cells[i].h * scale));
    }

    // Sort cells by group (stable sort preserves order within each group)
    std::stable_sort(layout.cells.begin(), layout.cells.end(),
        [](const AtlasCellInfo& a, const AtlasCellInfo& b) { return a.group < b.group; });

    // Lay out: each group gets its own row(s), wrapping within the group
    int atlasW = 0, atlasH = 0;
    int curX = 0, curY = 0, rowH = 0, col = 0;
    int prevGroup = -1;

    for (int i = 0; i < numPrims; i++)
    {
        // New group -> start a new row
        if (layout.cells[i].group != prevGroup)
        {
            if (prevGroup >= 0)
            {
                curX = 0;
                curY += rowH + ATLAS_PADDING * 2;
                rowH = 0;
                col = 0;
            }
            prevGroup = layout.cells[i].group;
        }

        // Wrap within group
        if (col >= ATLAS_GROUP_MAX_COLS)
        {
            curX = 0;
            curY += rowH + ATLAS_PADDING;
            rowH = 0;
            col = 0;
        }

        layout.cells[i].x = curX;
        layout.cells[i].y = curY;

        int cw = layout.cells[i].w;
        int ch = layout.cells[i].h;
        if (curX + cw + ATLAS_PADDING > atlasW) atlasW = curX + cw + ATLAS_PADDING;
        if (ch > rowH) rowH = ch;
        curX += cw + ATLAS_PADDING;
        col++;
    }
    atlasH = curY + rowH;

    // Round up to power of 2
    auto nextPow2 = [](int v) -> int {
        v--;
        v |= v >> 1; v |= v >> 2; v |= v >> 4; v |= v >> 8; v |= v >> 16;
        return v + 1;
    };
    layout.atlasW = nextPow2((std::max)(atlasW, 64));
    layout.atlasH = nextPow2((std::max)(atlasH, 64));

    return layout;
}

// Get atlas folder path
static std::string getAtlasFolder()
{
    std::string path(homePath);
    path += "atlases/";
    return path;
}

// Get atlas file path for a body, including HQR source name for uniqueness
static std::string getAtlasPath(const std::string& hqrName, int bodyNum)
{
    char buf[128];
    snprintf(buf, sizeof(buf), "body_%s_%03d.png", hqrName.c_str(), bodyNum);
    return getAtlasFolder() + buf;
}

// Get sphere atlas file path for a body
static std::string getSphereAtlasPath(const std::string& hqrName, int bodyNum)
{
    char buf[128];
    snprintf(buf, sizeof(buf), "spheres_%s_%03d.png", hqrName.c_str(), bodyNum);
    return getAtlasFolder() + buf;
}

// Get flat poly atlas file path for mixed models
static std::string getFlatPolyAtlasPath(const std::string& hqrName, int bodyNum)
{
    char buf[128];
    snprintf(buf, sizeof(buf), "flat_%s_%03d.png", hqrName.c_str(), bodyNum);
    return getAtlasFolder() + buf;
}

// Get ramp atlas file path for ramp-shaded polygons (material 3-6)
static std::string getRampAtlasPath(const std::string& hqrName, int bodyNum)
{
    char buf[128];
    snprintf(buf, sizeof(buf), "ramp_%s_%03d.png", hqrName.c_str(), bodyNum);
    return getAtlasFolder() + buf;
}

// Get other atlas file path for dither/transparent polygons (material 1-2)
static std::string getOtherAtlasPath(const std::string& hqrName, int bodyNum)
{
    char buf[128];
    snprintf(buf, sizeof(buf), "other_%s_%03d.png", hqrName.c_str(), bodyNum);
    return getAtlasFolder() + buf;
}

// Ensure the atlases directory exists
static void ensureAtlasDir()
{
#ifdef _WIN32
    std::string dir = getAtlasFolder();
    // Replace forward slashes for Windows
    for (auto& c : dir) { if (c == '/') c = '\\'; }
    CreateDirectoryA(dir.c_str(), NULL);
#else
    std::string dir = getAtlasFolder();
    mkdir(dir.c_str(), 0755);
#endif
}

// Generic helper to load atlas PNG data from atlases.hda archive or fall back to filesystem
// Attempts to load from archive using just the filename as entry name, with fallback to direct file loading
// Returns decompressed RGBA8 pixel data (caller must free with stbi_image_free)
// Sets w, h, channels on success; returns nullptr on failure
static unsigned char* loadAtlasDataFromArchiveOrFile(const std::string& filePath, int& w, int& h, int& channels)
{
    // Extract just the filename from the full path for archive entry lookup
    // (Archive entries use simple names like "body_hqrname_001.png", not full paths)
    size_t lastSlash = filePath.find_last_of("/\\");
    std::string fileName = (lastSlash != std::string::npos) ? filePath.substr(lastSlash + 1) : filePath;

    // First, try to load from atlases.hda archive
    if (HDArchive::open("atlases.hda"))
    {
        const HDArchiveEntry* entry = HDArchive::findEntry(fileName.c_str());
        if (entry)
        {
            size_t dataSize = 0;
            unsigned char* archiveData = HDArchive::readEntryAlloc(entry, &dataSize);
            HDArchive::close();

            if (archiveData && dataSize > 0)
            {
                // Use stbi_load_from_memory to decode the PNG from buffer
                unsigned char* pixels = stbi_load_from_memory(
                    archiveData, (int)dataSize, &w, &h, &channels, 4);

                free(archiveData);  // Free the compressed archive data

                if (pixels)
                {
                    // Note: uncomment for debug logging
                    // printf(RNDR_INFO "Loaded atlas from archive: %s (%dx%d)" CON_RESET "\n",
                    //     fileName.c_str(), w, h);
                    return pixels;
                }
            }
        }
        else
        {
            HDArchive::close();
        }
    }

    // Fallback: load from individual file on filesystem
    unsigned char* data = stbi_load(filePath.c_str(), &w, &h, &channels, 4);

    // Note: uncomment for debug logging if loaded from file
    // if (data)
    // {
    //     printf(RNDR_INFO "Loaded atlas from file: %s (%dx%d)" CON_RESET "\n",
    //         fileName.c_str(), w, h);
    // }

    return data;
}


// Compute a simple 2D bounding box size for a polygon face.
// Uses the first two edges to build a local 2D basis (flattening the 3D polygon).
// Returns estimated width and height in model-space units.
static void computePolyFlatSize(sBody* pBody, sPrimitive* pPrim, float& outW, float& outH)
{
    int nv = (int)pPrim->m_points.size();
    if (nv < 3) { outW = 1; outH = 1; return; }

    // Get 3D positions of first 3 vertices
    auto getVert = [&](int idx) -> point3dStruct& {
        return pBody->m_vertices[pPrim->m_points[idx]];
    };

    point3dStruct& v0 = getVert(0);
    point3dStruct& v1 = getVert(1);
    point3dStruct& v2 = getVert(2);

    // Edge vectors
    float e1x = (float)(v1.x - v0.x), e1y = (float)(v1.y - v0.y), e1z = (float)(v1.z - v0.z);
    float e2x = (float)(v2.x - v0.x), e2y = (float)(v2.y - v0.y), e2z = (float)(v2.z - v0.z);

    // Normal via cross product
    float nx = e1y * e2z - e1z * e2y;
    float ny = e1z * e2x - e1x * e2z;
    float nz = e1x * e2y - e1y * e2x;

    // Build local 2D basis: U = normalize(e1), V = normalize(N x U)
    float e1len = sqrtf(e1x * e1x + e1y * e1y + e1z * e1z);
    if (e1len < 0.001f) e1len = 1.f;
    float ux = e1x / e1len, uy = e1y / e1len, uz = e1z / e1len;

    // V = N x U (not normalized yet)
    float vx = ny * uz - nz * uy;
    float vy = nz * ux - nx * uz;
    float vz = nx * uy - ny * ux;
    float vlen = sqrtf(vx * vx + vy * vy + vz * vz);
    if (vlen < 0.001f) vlen = 1.f;
    vx /= vlen; vy /= vlen; vz /= vlen;

    // Project all vertices onto the UV plane
    float minU = 0, maxU = 0, minV = 0, maxV = 0;
    for (int i = 0; i < nv; i++)
    {
        point3dStruct& vi = getVert(i);
        float dx = (float)(vi.x - v0.x);
        float dy = (float)(vi.y - v0.y);
        float dz = (float)(vi.z - v0.z);
        float pu = dx * ux + dy * uy + dz * uz;
        float pv = dx * vx + dy * vy + dz * vz;
        if (pu < minU) minU = pu;
        if (pu > maxU) maxU = pu;
        if (pv < minV) minV = pv;
        if (pv > maxV) maxV = pv;
    }

    outW = maxU - minU;
    outH = maxV - minV;
    if (outW < 1.f) outW = 1.f;
    if (outH < 1.f) outH = 1.f;
}

// Project polygon vertices onto a 2D plane and return normalized [0..1] UVs relative to that plane's bounding box
static void projectPolyToUV(sBody* pBody, sPrimitive* pPrim, std::vector<float>& outU, std::vector<float>& outV)
{
    int nv = (int)pPrim->m_points.size();
    outU.resize(nv);
    outV.resize(nv);

    if (nv < 3)
    {
        for (int i = 0; i < nv; i++) { outU[i] = 0; outV[i] = 0; }
        return;
    }

    auto getVert = [&](int idx) -> point3dStruct& {
        return pBody->m_vertices[pPrim->m_points[idx]];
    };

    point3dStruct& v0 = getVert(0);
    point3dStruct& v1 = getVert(1);
    point3dStruct& v2 = getVert(2);

    float e1x = (float)(v1.x - v0.x), e1y = (float)(v1.y - v0.y), e1z = (float)(v1.z - v0.z);
    float e2x = (float)(v2.x - v0.x), e2y = (float)(v2.y - v0.y), e2z = (float)(v2.z - v0.z);

    float nx = e1y * e2z - e1z * e2y;
    float ny = e1z * e2x - e1x * e2z;
    float nz = e1x * e2y - e1y * e2x;

    float e1len = sqrtf(e1x * e1x + e1y * e1y + e1z * e1z);
    if (e1len < 0.001f) e1len = 1.f;
    float ux = e1x / e1len, uy = e1y / e1len, uz = e1z / e1len;

    float vx = ny * uz - nz * uy;
    float vy = nz * ux - nx * uz;
    float vz = nx * uy - ny * ux;
    float vlen = sqrtf(vx * vx + vy * vy + vz * vz);
    if (vlen < 0.001f) vlen = 1.f;
    vx /= vlen; vy /= vlen; vz /= vlen;

    float minU = 1e30f, maxU = -1e30f, minV = 1e30f, maxV = -1e30f;
    std::vector<float> projU(nv), projV(nv);

    for (int i = 0; i < nv; i++)
    {
        point3dStruct& vi = getVert(i);
        float dx = (float)(vi.x - v0.x);
        float dy = (float)(vi.y - v0.y);
        float dz = (float)(vi.z - v0.z);
        projU[i] = dx * ux + dy * uy + dz * uz;
        projV[i] = dx * vx + dy * vy + dz * vz;
        if (projU[i] < minU) minU = projU[i];
        if (projU[i] > maxU) maxU = projU[i];
        if (projV[i] < minV) minV = projV[i];
        if (projV[i] > maxV) maxV = projV[i];
    }

    float rangeU = maxU - minU;
    float rangeV = maxV - minV;
    if (rangeU < 0.001f) rangeU = 1.f;
    if (rangeV < 0.001f) rangeV = 1.f;

    for (int i = 0; i < nv; i++)
    {
        outU[i] = (projU[i] - minU) / rangeU;
        outV[i] = (projV[i] - minV) / rangeV;
    }
}

bool dumpModelAtlas(int bodyNum, sBody* pBody, const std::string& hqrName)
{
    if (!pBody) return false;

    int numPrims = (int)pBody->m_primitives.size();
    if (numPrims == 0) return false;

    // Compute rest-pose global vertices and projection parameters
    std::vector<point3dStruct> globalVerts;
    computeRestPoseVertices(pBody, globalVerts);
    ProjectionParams proj = computeProjectionParams(globalVerts);
    int atlasW = proj.atlasW;
    int atlasH = proj.atlasH;

    // Create the atlas image (RGBA)
    std::vector<unsigned char> image(atlasW * atlasH * 4, 0);

    // Rasterize each polygon using orthographic projection
    // Front-facing polys -> left half, back-facing polys -> right half (X-mirrored)
    int halfW = atlasW / 2;
    for (int i = 0; i < numPrims; i++)
    {
        sPrimitive* pPrim = &pBody->m_primitives[i];
        if (!isPrimPoly(pPrim)) continue;
        int nv = (int)pPrim->m_points.size();
        if (nv < 3) continue;

        unsigned char r = 128, g = 128, b = 128;
        int palIdx = pPrim->m_color;
        r = (unsigned char)RGB_Pal[palIdx * 3 + 0];
        g = (unsigned char)RGB_Pal[palIdx * 3 + 1];
        b = (unsigned char)RGB_Pal[palIdx * 3 + 2];

        bool front = isPolyFrontFacing(globalVerts, pPrim);

        // Project vertices to atlas pixel coordinates
        std::vector<float> px(nv), py(nv);
        for (int v = 0; v < nv; v++)
        {
            point3dStruct& vert = globalVerts[pPrim->m_points[v]];
            float xNorm = ((float)vert.x - proj.padMinX) / proj.rangeX;
            float yNorm = ((float)vert.y - proj.padMinY) / proj.rangeY;
            if (front)
                px[v] = xNorm * (float)halfW;
            else
                px[v] = (float)halfW + (1.0f - xNorm) * (float)halfW;
            py[v] = (1.0f - yNorm) * (float)atlasH;
        }

        // Fan-triangulate and fill
        for (int t = 1; t < nv - 1; t++)
        {
            fillTriangleInImage(image, atlasW, atlasH,
                px[0], py[0], px[t], py[t], px[t + 1], py[t + 1],
                r, g, b);
        }
    }

    // Write the PNG
    ensureAtlasDir();
    std::string path = getAtlasPath(hqrName, bodyNum);
    int result = stbi_write_png(path.c_str(), atlasW, atlasH, 4, image.data(), atlasW * 4);

    if (result)
    {
        printf(RNDR_TAG "Projection atlas dumped: %s (%dx%d, %d polys)\n", path.c_str(), atlasW, atlasH, numPrims);
    }
    else
    {
        printf(RNDR_ERR "Failed to write atlas: %s" CON_RESET "\n", path.c_str());
    }

    return result != 0;
}

ModelAtlasData* loadModelAtlas(int bodyNum, sBody* pBody, const std::string& hqrName)
{
    if (!pBody) return nullptr;

    // Check if already cached using composite key (HQR name + body number)
    std::string cacheKey = makeAtlasKey(hqrName, bodyNum);
    auto it = s_atlasCache.find(cacheKey);
    if (it != s_atlasCache.end())
        return &it->second;

    // Try to load the atlas PNG; auto-dump if it doesn't exist yet
    std::string path = getAtlasPath(hqrName, bodyNum);
    int w, h, channels;
    unsigned char* data = loadAtlasDataFromArchiveOrFile(path, w, h, channels);
    if (!data)
    {
        dumpModelAtlas(bodyNum, pBody, hqrName);
        data = loadAtlasDataFromArchiveOrFile(path, w, h, channels);
        if (!data)
            return nullptr;
    }

    printf(RNDR_TAG "Loading model atlas: %s (%dx%d)\n", path.c_str(), w, h);

    // Create bgfx texture
    ModelAtlasData atlas;
    atlas.atlasWidth = w;
    atlas.atlasHeight = h;

    // Keep CPU-side copy for transparency sampling in renderer
    atlas.pixels.assign(data, data + w * h * 4);

    const bgfx::Memory* mem = bgfx::copy(data, w * h * 4);
    atlas.texture = bgfx::createTexture2D(w, h, false, 1, bgfx::TextureFormat::RGBA8, 0, mem);
    stbi_image_free(data);

    if (!bgfx::isValid(atlas.texture))
    {
        printf(RNDR_ERR "Failed to create atlas texture for body %d" CON_RESET "\n", bodyNum);
        return nullptr;
    }

    // Compute projection UVs using the same rest-pose + projection as dump
    std::vector<point3dStruct> globalVerts;
    computeRestPoseVertices(pBody, globalVerts);
    ProjectionParams proj = computeProjectionParams(globalVerts);

    int numPrims = (int)pBody->m_primitives.size();
    atlas.polyUVs.resize(numPrims);

    for (int i = 0; i < numPrims; i++)
    {
        sPrimitive* pPrim = &pBody->m_primitives[i];
        if (!isPrimPoly(pPrim)) continue;

        int nv = (int)pPrim->m_points.size();
        atlas.polyUVs[i].u.resize(nv);
        atlas.polyUVs[i].v.resize(nv);

        bool front = isPolyFrontFacing(globalVerts, pPrim);

        for (int v = 0; v < nv; v++)
        {
            point3dStruct& vert = globalVerts[pPrim->m_points[v]];
            float xNorm = ((float)vert.x - proj.padMinX) / proj.rangeX;
            float yNorm = ((float)vert.y - proj.padMinY) / proj.rangeY;
            if (front)
                atlas.polyUVs[i].u[v] = xNorm * 0.5f;
            else
                atlas.polyUVs[i].u[v] = 0.5f + (1.0f - xNorm) * 0.5f;
            atlas.polyUVs[i].v[v] = 1.0f - yNorm;
        }
    }

    // Also try to load sphere atlas if it exists
    std::string spherePath = getSphereAtlasPath(hqrName, bodyNum);
    unsigned char* sphereData = loadAtlasDataFromArchiveOrFile(spherePath, w, h, channels);
    if (!sphereData)
    {
        // Auto-dump sphere atlas if it doesn't exist
        dumpSphereAtlas(bodyNum, pBody, hqrName);
        sphereData = loadAtlasDataFromArchiveOrFile(spherePath, w, h, channels);
    }

    if (sphereData)
    {
        printf(RNDR_TAG "Loading sphere atlas: %s (%dx%d)\n", spherePath.c_str(), w, h);
        atlas.sphereAtlasWidth = w;
        atlas.sphereAtlasHeight = h;

        const bgfx::Memory* sphereMem = bgfx::copy(sphereData, w * h * 4);
        atlas.sphereTexture = bgfx::createTexture2D(w, h, false, 1, bgfx::TextureFormat::RGBA8, 0, sphereMem);
        stbi_image_free(sphereData);

        if (!bgfx::isValid(atlas.sphereTexture))
        {
            printf(RNDR_WARN "Failed to create sphere atlas texture for body %d" CON_RESET "\n", bodyNum);
        }
    }

    // Also try to load flat poly atlas for mixed models (textured + flat-shaded)
    std::string flatPath = getFlatPolyAtlasPath(hqrName, bodyNum);
    unsigned char* flatData = loadAtlasDataFromArchiveOrFile(flatPath, w, h, channels);
    if (!flatData)
    {
        // Auto-dump flat poly atlas if it doesn't exist
        dumpFlatPolyAtlas(bodyNum, pBody, hqrName);
        flatData = loadAtlasDataFromArchiveOrFile(flatPath, w, h, channels);
    }

    if (flatData)
    {
        printf(RNDR_TAG "Loading flat poly atlas: %s (%dx%d)\n", flatPath.c_str(), w, h);
        atlas.flatAtlasWidth = w;
        atlas.flatAtlasHeight = h;

        // Keep CPU-side copy for transparency sampling in renderer
        atlas.flatPixels.assign(flatData, flatData + w * h * 4);

        const bgfx::Memory* flatMem = bgfx::copy(flatData, w * h * 4);
        atlas.flatTexture = bgfx::createTexture2D(w, h, false, 1, bgfx::TextureFormat::RGBA8, 0, flatMem);
        stbi_image_free(flatData);

        if (!bgfx::isValid(atlas.flatTexture))
        {
            printf(RNDR_WARN "Failed to create flat poly atlas texture for body %d" CON_RESET "\n", bodyNum);
        }
        else
        {
            // Compute UVs for flat polygons using same projection
            atlas.flatPolyUVs.resize(numPrims);
            for (int i = 0; i < numPrims; i++)
            {
                sPrimitive* pPrim = &pBody->m_primitives[i];
                if (pPrim->m_type != primTypeEnum_Poly) continue; // Only flat polygons

                int nv = (int)pPrim->m_points.size();
                atlas.flatPolyUVs[i].u.resize(nv);
                atlas.flatPolyUVs[i].v.resize(nv);

                bool front = isPolyFrontFacing(globalVerts, pPrim);

                for (int v = 0; v < nv; v++)
                {
                    point3dStruct& vert = globalVerts[pPrim->m_points[v]];
                    float xNorm = ((float)vert.x - proj.padMinX) / proj.rangeX;
                    float yNorm = ((float)vert.y - proj.padMinY) / proj.rangeY;
                    if (front)
                        atlas.flatPolyUVs[i].u[v] = xNorm * 0.5f;
                    else
                        atlas.flatPolyUVs[i].u[v] = 0.5f + (1.0f - xNorm) * 0.5f;
                    atlas.flatPolyUVs[i].v[v] = 1.0f - yNorm;
                }
            }
        }
    }

    // Also try to load ramp poly atlas for ramp-shaded polygons (material 3-6)
    std::string rampPath = getRampAtlasPath(hqrName, bodyNum);
    unsigned char* rampData = loadAtlasDataFromArchiveOrFile(rampPath, w, h, channels);
    if (!rampData)
    {
        // Auto-dump ramp poly atlas if it doesn't exist
        dumpRampAtlas(bodyNum, pBody, hqrName);
        rampData = loadAtlasDataFromArchiveOrFile(rampPath, w, h, channels);
    }

    if (rampData)
    {
        printf(RNDR_TAG "Loading ramp poly atlas: %s (%dx%d)\n", rampPath.c_str(), w, h);
        atlas.rampAtlasWidth = w;
        atlas.rampAtlasHeight = h;

        // Keep CPU-side copy for transparency sampling in renderer
        atlas.rampPixels.assign(rampData, rampData + w * h * 4);

        const bgfx::Memory* rampMem = bgfx::copy(rampData, w * h * 4);
        atlas.rampTexture = bgfx::createTexture2D(w, h, false, 1, bgfx::TextureFormat::RGBA8, 0, rampMem);
        stbi_image_free(rampData);

        if (!bgfx::isValid(atlas.rampTexture))
        {
            printf(RNDR_WARN "Failed to create ramp poly atlas texture for body %d" CON_RESET "\n", bodyNum);
        }
        else
        {
            // Compute UVs for ramp polygons using same 2-way split as flat atlas
            atlas.rampPolyUVs.resize(numPrims);
            for (int i = 0; i < numPrims; i++)
            {
                sPrimitive* pPrim = &pBody->m_primitives[i];
                if (!isPrimRamp(pPrim)) continue;

                int nv = (int)pPrim->m_points.size();
                atlas.rampPolyUVs[i].u.resize(nv);
                atlas.rampPolyUVs[i].v.resize(nv);

                bool isFront = isPolyFrontFacing(globalVerts, pPrim);

                for (int v = 0; v < nv; v++)
                {
                    point3dStruct& vert = globalVerts[pPrim->m_points[v]];
                    float xNorm = ((float)vert.x - proj.padMinX) / proj.rangeX;
                    float yNorm = ((float)vert.y - proj.padMinY) / proj.rangeY;

                    // Use exact same formula as flat atlas
                    if (isFront)
                        atlas.rampPolyUVs[i].u[v] = xNorm * 0.5f;
                    else
                        atlas.rampPolyUVs[i].u[v] = 0.5f + (1.0f - xNorm) * 0.5f;
                    atlas.rampPolyUVs[i].v[v] = 1.0f - yNorm;
                }
            }
        }
    }

    // Also try to load other poly atlas for dither/transparent polygons (material 1-2)
    std::string otherPath = getOtherAtlasPath(hqrName, bodyNum);
    unsigned char* otherData = loadAtlasDataFromArchiveOrFile(otherPath, w, h, channels);
    if (!otherData)
    {
        // Auto-dump other poly atlas if it doesn't exist
        dumpOtherAtlas(bodyNum, pBody, hqrName);
        otherData = loadAtlasDataFromArchiveOrFile(otherPath, w, h, channels);
    }

    if (otherData)
    {
        printf(RNDR_TAG "Loading other poly atlas: %s (%dx%d)\n", otherPath.c_str(), w, h);
        atlas.otherAtlasWidth = w;
        atlas.otherAtlasHeight = h;

        // Keep CPU-side copy for transparency sampling in renderer
        atlas.otherPixels.assign(otherData, otherData + w * h * 4);

        const bgfx::Memory* otherMem = bgfx::copy(otherData, w * h * 4);
        atlas.otherTexture = bgfx::createTexture2D(w, h, false, 1, bgfx::TextureFormat::RGBA8, 0, otherMem);
        stbi_image_free(otherData);

        if (!bgfx::isValid(atlas.otherTexture))
        {
            printf(RNDR_WARN "Failed to create other poly atlas texture for body %d" CON_RESET "\n", bodyNum);
        }
        else
        {
            // Compute UVs for other-material polygons using same 2-way split
            atlas.otherPolyUVs.resize(numPrims);
            for (int i = 0; i < numPrims; i++)
            {
                sPrimitive* pPrim = &pBody->m_primitives[i];
                if (!isPrimOther(pPrim)) continue;

                int nv = (int)pPrim->m_points.size();
                atlas.otherPolyUVs[i].u.resize(nv);
                atlas.otherPolyUVs[i].v.resize(nv);

                bool isFront = isPolyFrontFacing(globalVerts, pPrim);

                for (int v = 0; v < nv; v++)
                {
                    point3dStruct& vert = globalVerts[pPrim->m_points[v]];
                    float xNorm = ((float)vert.x - proj.padMinX) / proj.rangeX;
                    float yNorm = ((float)vert.y - proj.padMinY) / proj.rangeY;

                    if (isFront)
                        atlas.otherPolyUVs[i].u[v] = xNorm * 0.5f;
                    else
                        atlas.otherPolyUVs[i].u[v] = 0.5f + (1.0f - xNorm) * 0.5f;
                    atlas.otherPolyUVs[i].v[v] = 1.0f - yNorm;
                }
            }
        }
    }

    auto result = s_atlasCache.emplace(cacheKey, std::move(atlas));
    return &result.first->second;
}

bool dumpSphereAtlas(int bodyNum, sBody* pBody, const std::string& hqrName)
{
    if (!pBody) return false;

    int numPrims = (int)pBody->m_primitives.size();
    if (numPrims == 0) return false;

    // Count sphere primitives
    int numSpheres = 0;
    for (int i = 0; i < numPrims; i++)
    {
        if (isPrimSphere(&pBody->m_primitives[i]))
            numSpheres++;
    }

    if (numSpheres == 0)
        return false; // No spheres to dump

    // Create a simple atlas: each sphere gets a small circular billboard texture
    // We'll arrange them in a grid, each cell is 128x128 pixels
    const int cellSize = 128;
    const int cellsPerRow = 8;
    int numRows = (numSpheres + cellsPerRow - 1) / cellsPerRow;
    int atlasW = cellsPerRow * cellSize;
    int atlasH = numRows * cellSize;

    // Round up to power of 2
    auto nextPow2 = [](int v) -> int {
        v--;
        v |= v >> 1; v |= v >> 2; v |= v >> 4; v |= v >> 8; v |= v >> 16;
        return v + 1;
    };
    atlasW = nextPow2((std::max)(atlasW, 64));
    atlasH = nextPow2((std::max)(atlasH, 64));

    // Create the atlas image (RGBA)
    std::vector<unsigned char> image(atlasW * atlasH * 4, 0);

    // Render each sphere as a circular gradient billboard
    int sphereIdx = 0;
    for (int i = 0; i < numPrims; i++)
    {
        sPrimitive* pPrim = &pBody->m_primitives[i];
        if (!isPrimSphere(pPrim)) continue;

        // Get color from palette
        unsigned char r = 128, g = 128, b = 128;
        int palIdx = pPrim->m_color;
        r = (unsigned char)RGB_Pal[palIdx * 3 + 0];
        g = (unsigned char)RGB_Pal[palIdx * 3 + 1];
        b = (unsigned char)RGB_Pal[palIdx * 3 + 2];

        // Calculate cell position
        int cellX = (sphereIdx % cellsPerRow) * cellSize;
        int cellY = (sphereIdx / cellsPerRow) * cellSize;

        // Draw a circular billboard with radial gradient
        float centerX = cellX + cellSize * 0.5f;
        float centerY = cellY + cellSize * 0.5f;
        float radius = cellSize * 0.4f; // Leave some padding

        for (int y = 0; y < cellSize; y++)
        {
            for (int x = 0; x < cellSize; x++)
            {
                int pixelX = cellX + x;
                int pixelY = cellY + y;
                if (pixelX >= atlasW || pixelY >= atlasH) continue;

                float dx = (pixelX + 0.5f) - centerX;
                float dy = (pixelY + 0.5f) - centerY;
                float dist = sqrtf(dx * dx + dy * dy);

                if (dist <= radius)
                {
                    // Smooth circular falloff for billboard effect
                    float t = dist / radius;
                    float alpha = 1.0f - (t * t); // Quadratic falloff
                    alpha = (std::max)(0.0f, (std::min)(1.0f, alpha));

                    int offset = (pixelY * atlasW + pixelX) * 4;
                    image[offset + 0] = r;
                    image[offset + 1] = g;
                    image[offset + 2] = b;
                    image[offset + 3] = (unsigned char)(alpha * 255);
                }
            }
        }

        sphereIdx++;
    }

    // Write the PNG
    ensureAtlasDir();
    std::string path = getSphereAtlasPath(hqrName, bodyNum);
    int result = stbi_write_png(path.c_str(), atlasW, atlasH, 4, image.data(), atlasW * 4);

    if (result)
    {
        printf(RNDR_TAG "Sphere atlas dumped: %s (%dx%d, %d spheres)\n", path.c_str(), atlasW, atlasH, numSpheres);
    }
    else
    {
        printf(RNDR_ERR "Failed to write sphere atlas: %s" CON_RESET "\n", path.c_str());
    }

    return result != 0;
}

bool dumpFlatPolyAtlas(int bodyNum, sBody* pBody, const std::string& hqrName)
{
    if (!pBody) return false;

    int numPrims = (int)pBody->m_primitives.size();
    if (numPrims == 0) return false;

    // Check if this is a mixed model (has both textured and flat-shaded primitives)
    bool hasTextured = false;
    bool hasFlat = false;
    for (int i = 0; i < numPrims; i++)
    {
        sPrimitive* pPrim = &pBody->m_primitives[i];
        if (pPrim->m_type == processPrim_PolyTexture9 || pPrim->m_type == processPrim_PolyTexture10)
            hasTextured = true;
        if (pPrim->m_type == primTypeEnum_Poly)
            hasFlat = true;
    }

    if (!hasTextured || !hasFlat)
        return false; // Not a mixed model, nothing to dump

    // Use same projection system as main atlas
    std::vector<point3dStruct> globalVerts;
    computeRestPoseVertices(pBody, globalVerts);
    ProjectionParams proj = computeProjectionParams(globalVerts);
    int atlasW = proj.atlasW;
    int atlasH = proj.atlasH;

    // Create the atlas image (RGBA)
    std::vector<unsigned char> image(atlasW * atlasH * 4, 0);

    // Rasterize ONLY flat-shaded polygons (primTypeEnum_Poly)
    int halfW = atlasW / 2;
    int numFlat = 0;
    for (int i = 0; i < numPrims; i++)
    {
        sPrimitive* pPrim = &pBody->m_primitives[i];
        if (pPrim->m_type != primTypeEnum_Poly) continue; // Only flat polygons

        int nv = (int)pPrim->m_points.size();
        if (nv < 3) continue;

        unsigned char r = 128, g = 128, b = 128;
        int palIdx = pPrim->m_color;
        r = (unsigned char)RGB_Pal[palIdx * 3 + 0];
        g = (unsigned char)RGB_Pal[palIdx * 3 + 1];
        b = (unsigned char)RGB_Pal[palIdx * 3 + 2];

        bool front = isPolyFrontFacing(globalVerts, pPrim);

        // Project vertices to atlas pixel coordinates
        std::vector<float> px(nv), py(nv);
        for (int v = 0; v < nv; v++)
        {
            point3dStruct& vert = globalVerts[pPrim->m_points[v]];
            float xNorm = ((float)vert.x - proj.padMinX) / proj.rangeX;
            float yNorm = ((float)vert.y - proj.padMinY) / proj.rangeY;
            if (front)
                px[v] = xNorm * (float)halfW;
            else
                px[v] = (float)halfW + (1.0f - xNorm) * (float)halfW;
            py[v] = (1.0f - yNorm) * (float)atlasH;
        }

        // Fan-triangulate and fill
        for (int t = 1; t < nv - 1; t++)
        {
            fillTriangleInImage(image, atlasW, atlasH,
                px[0], py[0], px[t], py[t], px[t + 1], py[t + 1],
                r, g, b);
        }
        numFlat++;
    }

    if (numFlat == 0)
        return false; // No flat polygons found

    // Write the PNG
    ensureAtlasDir();
    std::string path = getFlatPolyAtlasPath(hqrName, bodyNum);
    int result = stbi_write_png(path.c_str(), atlasW, atlasH, 4, image.data(), atlasW * 4);

    if (result)
    {
        printf(RNDR_TAG "Flat poly atlas dumped: %s (%dx%d, %d flat polys)\n", path.c_str(), atlasW, atlasH, numFlat);
    }
    else
    {
        printf(RNDR_ERR "Failed to write flat poly atlas: %s" CON_RESET "\n", path.c_str());
    }

    return result != 0;
}

bool dumpRampAtlas(int bodyNum, sBody* pBody, const std::string& hqrName)
{
    if (!pBody) return false;

    int numPrims = (int)pBody->m_primitives.size();
    if (numPrims == 0) return false;

    // Check if this model has any ramp-shaded polygons (material 3-6)
    bool hasRamp = false;
    for (int i = 0; i < numPrims; i++)
    {
        if (isPrimRamp(&pBody->m_primitives[i]))
        {
            hasRamp = true;
            break;
        }
    }

    if (!hasRamp)
        return false; // No ramp polygons to dump

    // Use same projection system as main atlas (2-way split like flat atlas)
    std::vector<point3dStruct> globalVerts;
    computeRestPoseVertices(pBody, globalVerts);
    ProjectionParams proj = computeProjectionParams(globalVerts);
    int atlasW = proj.atlasW;
    int atlasH = proj.atlasH;

    // Create the atlas image (RGBA)
    std::vector<unsigned char> image(atlasW * atlasH * 4, 0);

    // Rasterize ONLY ramp-shaded polygons (material 3-6) - 2-way split like flat atlas
    int halfW = atlasW / 2;
    int numRamp = 0;
    for (int i = 0; i < numPrims; i++)
    {
        sPrimitive* pPrim = &pBody->m_primitives[i];
        if (!isPrimRamp(pPrim)) continue;

        int nv = (int)pPrim->m_points.size();
        if (nv < 3) continue;

        unsigned char r = 128, g = 128, b = 128;
        int palIdx = pPrim->m_color;
        r = (unsigned char)RGB_Pal[palIdx * 3 + 0];
        g = (unsigned char)RGB_Pal[palIdx * 3 + 1];
        b = (unsigned char)RGB_Pal[palIdx * 3 + 2];

        // Determine front vs back facing (2-way split like flat atlas)
        bool isFront = isPolyFrontFacing(globalVerts, pPrim);

        // Project vertices to atlas pixel coordinates
        std::vector<float> px(nv), py(nv);
        for (int v = 0; v < nv; v++)
        {
            point3dStruct& vert = globalVerts[pPrim->m_points[v]];
            float xNorm = ((float)vert.x - proj.padMinX) / proj.rangeX;
            float yNorm = ((float)vert.y - proj.padMinY) / proj.rangeY;
            if (isFront)
                px[v] = xNorm * (float)halfW;
            else
                px[v] = (float)halfW + (1.0f - xNorm) * (float)halfW;
            py[v] = (1.0f - yNorm) * (float)atlasH;
        }

        // Fan-triangulate and fill
        for (int t = 1; t < nv - 1; t++)
        {
            fillTriangleInImage(image, atlasW, atlasH,
                px[0], py[0], px[t], py[t], px[t + 1], py[t + 1],
                r, g, b);
        }
        numRamp++;
    }

    if (numRamp == 0)
        return false; // No ramp polygons found

    // Write the PNG
    ensureAtlasDir();
    std::string path = getRampAtlasPath(hqrName, bodyNum);
    int result = stbi_write_png(path.c_str(), atlasW, atlasH, 4, image.data(), atlasW * 4);

    if (result)
    {
        printf(RNDR_TAG "Ramp poly atlas dumped (2-direction): %s (%dx%d, %d ramp polys)\n", path.c_str(), atlasW, atlasH, numRamp);
    }
    else
    {
        printf(RNDR_ERR "Failed to write ramp poly atlas: %s" CON_RESET "\n", path.c_str());
    }

    return result != 0;
}

bool dumpOtherAtlas(int bodyNum, sBody* pBody, const std::string& hqrName)
{
    if (!pBody) return false;

    int numPrims = (int)pBody->m_primitives.size();
    if (numPrims == 0) return false;

    // Check if this model has any other-material polygons (material 1-2)
    bool hasOther = false;
    for (int i = 0; i < numPrims; i++)
    {
        if (isPrimOther(&pBody->m_primitives[i]))
        {
            hasOther = true;
            break;
        }
    }

    if (!hasOther)
        return false;

    // Use same projection system as main atlas (2-way split)
    std::vector<point3dStruct> globalVerts;
    computeRestPoseVertices(pBody, globalVerts);
    ProjectionParams proj = computeProjectionParams(globalVerts);
    int atlasW = proj.atlasW;
    int atlasH = proj.atlasH;

    // Create the atlas image (RGBA)
    std::vector<unsigned char> image(atlasW * atlasH * 4, 0);

    // Rasterize ONLY other-material polygons (material 1-2) - 2-way split like flat atlas
    int halfW = atlasW / 2;
    int numOther = 0;
    for (int i = 0; i < numPrims; i++)
    {
        sPrimitive* pPrim = &pBody->m_primitives[i];
        if (!isPrimOther(pPrim)) continue;

        int nv = (int)pPrim->m_points.size();
        if (nv < 3) continue;

        unsigned char r = 128, g = 128, b = 128;
        int palIdx = pPrim->m_color;
        r = (unsigned char)RGB_Pal[palIdx * 3 + 0];
        g = (unsigned char)RGB_Pal[palIdx * 3 + 1];
        b = (unsigned char)RGB_Pal[palIdx * 3 + 2];

        bool isFront = isPolyFrontFacing(globalVerts, pPrim);

        std::vector<float> px(nv), py(nv);
        for (int v = 0; v < nv; v++)
        {
            point3dStruct& vert = globalVerts[pPrim->m_points[v]];
            float xNorm = ((float)vert.x - proj.padMinX) / proj.rangeX;
            float yNorm = ((float)vert.y - proj.padMinY) / proj.rangeY;
            if (isFront)
                px[v] = xNorm * (float)halfW;
            else
                px[v] = (float)halfW + (1.0f - xNorm) * (float)halfW;
            py[v] = (1.0f - yNorm) * (float)atlasH;
        }

        for (int t = 1; t < nv - 1; t++)
        {
            fillTriangleInImage(image, atlasW, atlasH,
                px[0], py[0], px[t], py[t], px[t + 1], py[t + 1],
                r, g, b);
        }
        numOther++;
    }

    if (numOther == 0)
        return false;

    ensureAtlasDir();
    std::string path = getOtherAtlasPath(hqrName, bodyNum);
    int result = stbi_write_png(path.c_str(), atlasW, atlasH, 4, image.data(), atlasW * 4);

    if (result)
    {
        printf(RNDR_TAG "Other poly atlas dumped (2-direction): %s (%dx%d, %d other polys)\n", path.c_str(), atlasW, atlasH, numOther);
    }
    else
    {
        printf(RNDR_ERR "Failed to write other poly atlas: %s" CON_RESET "\n", path.c_str());
    }

    return result != 0;
}

void clearModelAtlases()
{
    for (auto& pair : s_atlasCache)
    {
        if (bgfx::isValid(pair.second.texture))
            bgfx::destroy(pair.second.texture);
        if (bgfx::isValid(pair.second.sphereTexture))
            bgfx::destroy(pair.second.sphereTexture);
        if (bgfx::isValid(pair.second.flatTexture))
            bgfx::destroy(pair.second.flatTexture);
        if (bgfx::isValid(pair.second.rampTexture))
            bgfx::destroy(pair.second.rampTexture);
        if (bgfx::isValid(pair.second.otherTexture))
            bgfx::destroy(pair.second.otherTexture);
    }
    s_atlasCache.clear();
}
