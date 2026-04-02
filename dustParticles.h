///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Copyright (C) 2026 Infogrames / Spacefarer Retro Remasters LLC
// Based on FITD by yaz0r, Re-haunted is released under GPL
// Author: AI Assistant
//
// Atmospheric dust particle system for the attic (ETAGE 7)
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include <bgfx/bgfx.h>

// Dust particle system for atmospheric effects in the attic
class DustParticleSystem
{
public:
    DustParticleSystem();
    ~DustParticleSystem();

    void init();
    void shutdown();
    void update(float deltaTime);
    void render(bgfx::ViewId viewId);

    void setEnabled(bool enabled) { m_enabled = enabled; }
    bool isEnabled() const { return m_enabled; }

private:
    struct Particle
    {
        float x, y, z;          // Position
        float vx, vy, vz;       // Velocity
        float size;             // Particle size
        float alpha;            // Transparency
        float life;             // Remaining lifetime
        float maxLife;          // Initial lifetime
    };

    void createParticles();
    void updateParticle(Particle& p, float deltaTime);

    static const int MAX_PARTICLES = 150;
    Particle m_particles[MAX_PARTICLES];
    
    bgfx::VertexBufferHandle m_vbh = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle m_program = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_u_params = BGFX_INVALID_HANDLE;
    
    bool m_enabled = false;
    bool m_initialized = false;
    float m_spawnTimer = 0.0f;
};

// Global instance
extern DustParticleSystem* g_dustParticles;
