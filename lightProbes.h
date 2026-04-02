///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Copyright (C) 2026 Infogrames / Spacefarer Retro Remasters LLC
// Based on FITD by yaz0r, Re-haunted is released under GPL
// Author: Jake Jackson (jake@spacefarergames.com)
//
// Light Probe system for ambient lighting using Spherical Harmonics
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include <vector>
#include <array>

// Spherical Harmonics coefficient structure (3 bands = 9 coefficients per RGB channel)
struct SHCoefficients {
    std::array<float, 9> r; // Red channel SH coefficients
    std::array<float, 9> g; // Green channel SH coefficients
    std::array<float, 9> b; // Blue channel SH coefficients

    SHCoefficients() {
        r.fill(0.0f);
        g.fill(0.0f);
        b.fill(0.0f);
    }
};

// Light probe structure - captures ambient lighting at a specific world position
struct LightProbe {
    float x, y, z;              // World position
    SHCoefficients sh;          // Spherical harmonics coefficients
    int floor;                  // Floor/stage index
    int camera;                 // Camera index (-1 = global for floor)
    
    LightProbe() : x(0), y(0), z(0), floor(0), camera(-1) {}
};

// Light probe manager - handles probe loading, saving, and runtime sampling
class LightProbeManager {
public:
    LightProbeManager();
    ~LightProbeManager();

    // Initialization
    void init();
    void shutdown();

    // Probe management
    bool loadProbes(int floor, int camera = -1);
    bool saveProbes(int floor, int camera = -1);
    void clearProbes();

    // Add a probe manually (for editor/debug)
    void addProbe(float x, float y, float z, int floor, int camera = -1);

    // Runtime sampling - get ambient color at a world position
    // Returns RGB color from nearest probe(s) using SH evaluation
    void sampleProbes(float x, float y, float z, float& r, float& g, float& b);

    // Get all probes (for debug visualization)
    const std::vector<LightProbe>& getProbes() const { return m_probes; }

    // Enable/disable light probes
    void setEnabled(bool enabled) { m_enabled = enabled; }
    bool isEnabled() const { return m_enabled; }

    // Set intensity multiplier
    void setIntensity(float intensity) { m_intensity = intensity; }
    float getIntensity() const { return m_intensity; }

private:
    // Find nearest probes for blending
    void findNearestProbes(float x, float y, float z, int maxProbes, std::vector<int>& indices, std::vector<float>& weights);

    // Evaluate SH for a given direction (normalized vector)
    void evaluateSH(const SHCoefficients& sh, float nx, float ny, float nz, float& r, float& g, float& b);

    // Generate default probe (simple white ambient light)
    void createDefaultProbe(LightProbe& probe);

    std::vector<LightProbe> m_probes;
    bool m_enabled = true;
    float m_intensity = 0.5f;
    int m_currentFloor = -1;
    int m_currentCamera = -1;
};

// Global instance
extern LightProbeManager* g_lightProbeManager;
