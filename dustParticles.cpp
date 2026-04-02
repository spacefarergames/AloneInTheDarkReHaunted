///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Copyright (C) 2026 Infogrames / Spacefarer Retro Remasters LLC
// Based on FITD by yaz0r, Re-haunted is released under GPL
// Author: AI Assistant
//
// Atmospheric dust particle system implementation
///////////////////////////////////////////////////////////////////////////////

// Prevent Windows min/max macro conflicts
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "common.h"
#include "dustParticles.h"
#include <stdlib.h>
#include <math.h>
#include <bgfx/bgfx.h>
#include <bx/math.h>

extern bgfx::ProgramHandle loadBgfxProgram(const std::string& VSFile, const std::string& PSFile);

DustParticleSystem* g_dustParticles = nullptr;

// Vertex structure for particles (point sprites)
struct ParticleVertex
{
    float x, y, z;
    float alpha;

    static void init()
    {
        ms_layout
            .begin()
            .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
            .add(bgfx::Attrib::Color0, 1, bgfx::AttribType::Float)
            .end();
    }

    static bgfx::VertexLayout ms_layout;
};

bgfx::VertexLayout ParticleVertex::ms_layout;

// Simple random float between min and max
static float randomFloat(float min, float max)
{
    return min + (max - min) * ((float)rand() / (float)RAND_MAX);
}

DustParticleSystem::DustParticleSystem()
{
}

DustParticleSystem::~DustParticleSystem()
{
    shutdown();
}

void DustParticleSystem::init()
{
    if (m_initialized)
        return;

    ParticleVertex::init();

    // Load particle shader
    m_program = loadBgfxProgram("particle_vs", "particle_ps");

    // Create particles with random initial positions in screen-space
    for (int i = 0; i < MAX_PARTICLES; i++)
    {
        Particle& p = m_particles[i];

        // Screen-space coordinates (0-320 x, 0-200 y, 500-950 z for depth layering)
        p.x = randomFloat(0.0f, 320.0f);
        p.y = randomFloat(0.0f, 200.0f);
        p.z = randomFloat(500.0f, 950.0f);

        // Very slow drift in screen space
        p.vx = randomFloat(-5.0f, 5.0f);
        p.vy = randomFloat(-10.0f, -2.0f);  // Slight downward drift
        p.vz = randomFloat(-10.0f, 10.0f);

        // Random size and alpha
        p.size = randomFloat(0.5f, 2.0f);
        p.alpha = randomFloat(0.15f, 0.35f);

        // Random lifetime
        p.maxLife = randomFloat(10.0f, 20.0f);
        p.life = randomFloat(0.0f, p.maxLife);
    }

    m_initialized = true;
}

void DustParticleSystem::shutdown()
{
    if (!m_initialized)
        return;

    if (bgfx::isValid(m_program))
    {
        bgfx::destroy(m_program);
        m_program = BGFX_INVALID_HANDLE;
    }

    m_initialized = false;
}

void DustParticleSystem::updateParticle(Particle& p, float deltaTime)
{
    // Update position in screen-space
    p.x += p.vx * deltaTime;
    p.y += p.vy * deltaTime;
    p.z += p.vz * deltaTime;

    // Add gentle sine wave motion for floating effect
    static float timeAccum = 0.0f;
    timeAccum += deltaTime;
    p.x += sin(timeAccum * 0.3f + p.y * 0.01f) * 2.0f * deltaTime;

    // Update lifetime
    p.life -= deltaTime;

    // Respawn particle if dead or out of screen bounds
    if (p.life <= 0.0f || 
        p.y < -10.0f || p.y > 210.0f ||
        p.x < -10.0f || p.x > 330.0f ||
        p.z < 400.0f || p.z > 1000.0f)
    {
        // Reset particle at top of screen
        p.x = randomFloat(0.0f, 320.0f);
        p.y = randomFloat(-20.0f, 0.0f);
        p.z = randomFloat(500.0f, 950.0f);
        p.life = p.maxLife;
    }

    // Fade in/out based on lifetime
    float lifeFraction = p.life / p.maxLife;
    if (lifeFraction > 0.85f)
    {
        // Fade in at start
        p.alpha = (1.0f - lifeFraction) * 5.0f * randomFloat(0.15f, 0.35f);
    }
    else if (lifeFraction < 0.15f)
    {
        // Fade out at end
        p.alpha = lifeFraction * (1.0f / 0.15f) * randomFloat(0.15f, 0.35f);
    }
}

void DustParticleSystem::update(float deltaTime)
{
    if (!m_enabled || !m_initialized)
        return;

    // Update all particles
    for (int i = 0; i < MAX_PARTICLES; i++)
    {
        updateParticle(m_particles[i], deltaTime);
    }
}

void DustParticleSystem::render(bgfx::ViewId viewId)
{
    if (!m_enabled || !m_initialized)
        return;

    if (!bgfx::isValid(m_program))
        return;

    // Render each particle as a small quad (2 triangles)
    // We'll create vertices on-the-fly for each visible particle

    int numQuads = 0;
    for (int i = 0; i < MAX_PARTICLES; i++)
    {
        if (m_particles[i].alpha > 0.01f)
            numQuads++;
    }

    if (numQuads == 0)
        return;

    uint32_t numVertices = numQuads * 6; // 6 vertices per quad (2 triangles)

    // Check if we can allocate transient buffer
    if (bgfx::getAvailTransientVertexBuffer(numVertices, ParticleVertex::ms_layout) < numVertices)
        return;

    // Build vertex buffer with particle quads
    bgfx::TransientVertexBuffer tvb;
    bgfx::allocTransientVertexBuffer(&tvb, numVertices, ParticleVertex::ms_layout);

    ParticleVertex* vertices = (ParticleVertex*)tvb.data;
    int vertexIdx = 0;

    const float particleSize = 1.5f; // Small dust particle size

    for (int i = 0; i < MAX_PARTICLES; i++)
    {
        const Particle& p = m_particles[i];
        if (p.alpha < 0.01f)
            continue;

        float px = p.x;
        float py = p.y;
        float pz = p.z;
        float size = particleSize;

        // Create a small quad around the particle center
        // Triangle 1
        vertices[vertexIdx].x = px - size; vertices[vertexIdx].y = py - size; vertices[vertexIdx].z = pz;
        vertices[vertexIdx].alpha = p.alpha;
        vertexIdx++;

        vertices[vertexIdx].x = px + size; vertices[vertexIdx].y = py - size; vertices[vertexIdx].z = pz;
        vertices[vertexIdx].alpha = p.alpha;
        vertexIdx++;

        vertices[vertexIdx].x = px + size; vertices[vertexIdx].y = py + size; vertices[vertexIdx].z = pz;
        vertices[vertexIdx].alpha = p.alpha;
        vertexIdx++;

        // Triangle 2
        vertices[vertexIdx].x = px - size; vertices[vertexIdx].y = py - size; vertices[vertexIdx].z = pz;
        vertices[vertexIdx].alpha = p.alpha;
        vertexIdx++;

        vertices[vertexIdx].x = px + size; vertices[vertexIdx].y = py + size; vertices[vertexIdx].z = pz;
        vertices[vertexIdx].alpha = p.alpha;
        vertexIdx++;

        vertices[vertexIdx].x = px - size; vertices[vertexIdx].y = py + size; vertices[vertexIdx].z = pz;
        vertices[vertexIdx].alpha = p.alpha;
        vertexIdx++;
    }

    // Set state for transparent particles
    bgfx::setState(0
        | BGFX_STATE_WRITE_RGB
        | BGFX_STATE_WRITE_A
        | BGFX_STATE_BLEND_ALPHA
        | BGFX_STATE_DEPTH_TEST_LEQUAL
        | BGFX_STATE_MSAA
    );

    bgfx::setVertexBuffer(0, &tvb);
    bgfx::submit(viewId, m_program);
}
