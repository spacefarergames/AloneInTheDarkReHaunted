///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Copyright (C) 2026 Infogrames / Spacefarer Retro Remasters LLC
// Based on FITD by yaz0r, Re-haunted is released under GPL
// Author: Jake Jackson (jake@spacefarergames.com)
//
// Light Probe system implementation
///////////////////////////////////////////////////////////////////////////////

#include "common.h"
#include "lightProbes.h"
#include "consoleLog.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <algorithm>

LightProbeManager* g_lightProbeManager = nullptr;

LightProbeManager::LightProbeManager()
{
}

LightProbeManager::~LightProbeManager()
{
    shutdown();
}

void LightProbeManager::init()
{
    m_enabled = true;
    m_intensity = 0.5f;
    m_currentFloor = -1;
    m_currentCamera = -1;
}

void LightProbeManager::shutdown()
{
    clearProbes();
}

void LightProbeManager::clearProbes()
{
    m_probes.clear();
}

bool LightProbeManager::loadProbes(int floor, int camera)
{
    char filename[256];
    if (camera >= 0)
        sprintf(filename, "probes_floor%02d_cam%02d.dat", floor, camera);
    else
        sprintf(filename, "probes_floor%02d.dat", floor);

    FILE* file = fopen(filename, "rb");
    if (!file)
    {
        // No probes file found - create default ambient probe
        printf("PROBE: No probe file found for floor %d camera %d, using default ambient\n", floor, camera);
        
        // Clear existing probes for this floor/camera
        m_probes.erase(
            std::remove_if(m_probes.begin(), m_probes.end(),
                [floor, camera](const LightProbe& p) { return p.floor == floor && p.camera == camera; }),
            m_probes.end()
        );

        // Add a single default probe
        LightProbe defaultProbe;
        defaultProbe.x = 0.0f;
        defaultProbe.y = 0.0f;
        defaultProbe.z = 0.0f;
        defaultProbe.floor = floor;
        defaultProbe.camera = camera;
        createDefaultProbe(defaultProbe);
        m_probes.push_back(defaultProbe);

        m_currentFloor = floor;
        m_currentCamera = camera;
        return false;
    }

    // Read probe count
    int probeCount = 0;
    fread(&probeCount, sizeof(int), 1, file);

    if (probeCount <= 0 || probeCount > 1000)
    {
        printf("PROBE: Invalid probe count %d in file %s\n", probeCount, filename);
        fclose(file);
        return false;
    }

    // Clear existing probes for this floor/camera
    m_probes.erase(
        std::remove_if(m_probes.begin(), m_probes.end(),
            [floor, camera](const LightProbe& p) { return p.floor == floor && p.camera == camera; }),
        m_probes.end()
    );

    // Read probes
    for (int i = 0; i < probeCount; i++)
    {
        LightProbe probe;
        fread(&probe.x, sizeof(float), 1, file);
        fread(&probe.y, sizeof(float), 1, file);
        fread(&probe.z, sizeof(float), 1, file);
        fread(probe.sh.r.data(), sizeof(float), 9, file);
        fread(probe.sh.g.data(), sizeof(float), 9, file);
        fread(probe.sh.b.data(), sizeof(float), 9, file);
        probe.floor = floor;
        probe.camera = camera;
        m_probes.push_back(probe);
    }

    fclose(file);

    printf("PROBE: Loaded %d probes for floor %d camera %d\n", probeCount, floor, camera);
    m_currentFloor = floor;
    m_currentCamera = camera;
    return true;
}

bool LightProbeManager::saveProbes(int floor, int camera)
{
    char filename[256];
    if (camera >= 0)
        sprintf(filename, "probes_floor%02d_cam%02d.dat", floor, camera);
    else
        sprintf(filename, "probes_floor%02d.dat", floor);

    FILE* file = fopen(filename, "wb");
    if (!file)
    {
        printf("PROBE: Failed to save probes to %s\n", filename);
        return false;
    }

    // Count probes for this floor/camera
    int probeCount = 0;
    for (const auto& probe : m_probes)
    {
        if (probe.floor == floor && probe.camera == camera)
            probeCount++;
    }

    // Write probe count
    fwrite(&probeCount, sizeof(int), 1, file);

    // Write probes
    for (const auto& probe : m_probes)
    {
        if (probe.floor == floor && probe.camera == camera)
        {
            fwrite(&probe.x, sizeof(float), 1, file);
            fwrite(&probe.y, sizeof(float), 1, file);
            fwrite(&probe.z, sizeof(float), 1, file);
            fwrite(probe.sh.r.data(), sizeof(float), 9, file);
            fwrite(probe.sh.g.data(), sizeof(float), 9, file);
            fwrite(probe.sh.b.data(), sizeof(float), 9, file);
        }
    }

    fclose(file);
    printf("PROBE: Saved %d probes for floor %d camera %d\n", probeCount, floor, camera);
    return true;
}

void LightProbeManager::addProbe(float x, float y, float z, int floor, int camera)
{
    LightProbe probe;
    probe.x = x;
    probe.y = y;
    probe.z = z;
    probe.floor = floor;
    probe.camera = camera;
    createDefaultProbe(probe);
    m_probes.push_back(probe);
}

void LightProbeManager::findNearestProbes(float x, float y, float z, int maxProbes, std::vector<int>& indices, std::vector<float>& weights)
{
    indices.clear();
    weights.clear();

    if (m_probes.empty())
        return;

    // Calculate distances to all probes in current floor/camera
    struct ProbeDistance {
        int index;
        float distance;
    };
    std::vector<ProbeDistance> distances;

    for (int i = 0; i < (int)m_probes.size(); i++)
    {
        const LightProbe& probe = m_probes[i];
        
        // Only consider probes from current floor
        if (probe.floor != m_currentFloor)
            continue;

        // If camera-specific probes exist, prefer those
        if (m_currentCamera >= 0 && probe.camera != m_currentCamera && probe.camera != -1)
            continue;

        float dx = probe.x - x;
        float dy = probe.y - y;
        float dz = probe.z - z;
        float dist = sqrtf(dx * dx + dy * dy + dz * dz);
        
        distances.push_back({ i, dist });
    }

    if (distances.empty())
        return;

    // Sort by distance
    std::sort(distances.begin(), distances.end(),
        [](const ProbeDistance& a, const ProbeDistance& b) { return a.distance < b.distance; });

    // Take nearest maxProbes probes
    int numProbes = (std::min)((int)distances.size(), maxProbes);
    
    // Calculate inverse distance weights
    float totalWeight = 0.0f;
    for (int i = 0; i < numProbes; i++)
    {
        float dist = distances[i].distance;
        // Avoid division by zero for probes at exact position
        float weight = 1.0f / (dist + 0.1f);
        indices.push_back(distances[i].index);
        weights.push_back(weight);
        totalWeight += weight;
    }

    // Normalize weights
    if (totalWeight > 0.0f)
    {
        for (float& w : weights)
            w /= totalWeight;
    }
}

void LightProbeManager::evaluateSH(const SHCoefficients& sh, float nx, float ny, float nz, float& r, float& g, float& b)
{
    // SH basis functions (3 bands = 9 coefficients)
    // L0 (constant)
    float sh0 = 0.282095f;
    
    // L1 (linear)
    float sh1 = 0.488603f * ny;
    float sh2 = 0.488603f * nz;
    float sh3 = 0.488603f * nx;
    
    // L2 (quadratic)
    float sh4 = 1.092548f * nx * ny;
    float sh5 = 1.092548f * ny * nz;
    float sh6 = 0.315392f * (3.0f * nz * nz - 1.0f);
    float sh7 = 1.092548f * nx * nz;
    float sh8 = 0.546274f * (nx * nx - ny * ny);

    // Evaluate SH for each channel
    r = sh.r[0] * sh0 +
        sh.r[1] * sh1 + sh.r[2] * sh2 + sh.r[3] * sh3 +
        sh.r[4] * sh4 + sh.r[5] * sh5 + sh.r[6] * sh6 + sh.r[7] * sh7 + sh.r[8] * sh8;
    
    g = sh.g[0] * sh0 +
        sh.g[1] * sh1 + sh.g[2] * sh2 + sh.g[3] * sh3 +
        sh.g[4] * sh4 + sh.g[5] * sh5 + sh.g[6] * sh6 + sh.g[7] * sh7 + sh.g[8] * sh8;
    
    b = sh.b[0] * sh0 +
        sh.b[1] * sh1 + sh.b[2] * sh2 + sh.b[3] * sh3 +
        sh.b[4] * sh4 + sh.b[5] * sh5 + sh.b[6] * sh6 + sh.b[7] * sh7 + sh.b[8] * sh8;

    // Clamp to valid range
    r = (std::max)(0.0f, (std::min)(1.0f, r));
    g = (std::max)(0.0f, (std::min)(1.0f, g));
    b = (std::max)(0.0f, (std::min)(1.0f, b));
}

void LightProbeManager::sampleProbes(float x, float y, float z, float& r, float& g, float& b)
{
    r = g = b = 0.0f;

    if (!m_enabled || m_probes.empty())
    {
        // Default ambient
        r = g = b = 0.3f;
        return;
    }

    // Find nearest 4 probes and blend
    std::vector<int> indices;
    std::vector<float> weights;
    findNearestProbes(x, y, z, 4, indices, weights);

    if (indices.empty())
    {
        // No probes found, use default ambient
        r = g = b = 0.3f;
        return;
    }

    // For simplicity, we'll use the average up direction (0, 1, 0) for SH evaluation
    // In a more advanced system, you'd use the surface normal
    float nx = 0.0f, ny = 1.0f, nz = 0.0f;

    // Blend probe contributions
    for (size_t i = 0; i < indices.size(); i++)
    {
        const LightProbe& probe = m_probes[indices[i]];
        float pr, pg, pb;
        evaluateSH(probe.sh, nx, ny, nz, pr, pg, pb);
        
        r += pr * weights[i];
        g += pg * weights[i];
        b += pb * weights[i];
    }

    // Apply intensity
    r *= m_intensity;
    g *= m_intensity;
    b *= m_intensity;
}

void LightProbeManager::createDefaultProbe(LightProbe& probe)
{
    // Create a simple white ambient probe
    // Only the DC coefficient (sh0) is non-zero for uniform ambient
    float ambientLevel = 0.4f; // Moderate ambient light
    
    // SH DC coefficient (constant term)
    probe.sh.r[0] = ambientLevel;
    probe.sh.g[0] = ambientLevel;
    probe.sh.b[0] = ambientLevel;
    
    // All other coefficients are zero (no directional variation)
    for (int i = 1; i < 9; i++)
    {
        probe.sh.r[i] = 0.0f;
        probe.sh.g[i] = 0.0f;
        probe.sh.b[i] = 0.0f;
    }
}
