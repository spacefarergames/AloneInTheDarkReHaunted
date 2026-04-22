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
    float isDirt;  // 0.0 = white dust, 1.0 = brown dirt

    static void init()
    {
        ms_layout
            .begin()
            .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
            .add(bgfx::Attrib::Color0, 1, bgfx::AttribType::Float)
            .add(bgfx::Attrib::Color1, 1, bgfx::AttribType::Float)
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

    // Debug: Verify shader loaded
    if (!bgfx::isValid(m_program))
    {
        return;
    }

    // Create particles with random initial positions in screen-space
    for (int i = 0; i < MAX_PARTICLES; i++)
    {
        Particle& p = m_particles[i];

        // Screen-space coordinates - START IN CENTER FOR TESTING
        p.x = randomFloat(150.0f, 170.0f);  // Center screen X
        p.y = randomFloat(90.0f, 110.0f);   // Center screen Y
        p.z = randomFloat(500.0f, 950.0f);

        // Very slow drift in screen space
        p.vx = randomFloat(-5.0f, 5.0f);
        p.vy = randomFloat(-10.0f, -2.0f);  // Slight downward drift
        p.vz = randomFloat(-10.0f, 10.0f);

        // Random size and alpha
        p.size = randomFloat(0.5f, 2.0f);
        p.alpha = randomFloat(0.7f, 1.0f);  // Full opacity for testing

        // Random lifetime
        p.maxLife = randomFloat(10.0f, 20.0f);
        p.life = randomFloat(0.0f, p.maxLife);

        // Atmospheric particles are white dust
        p.isDirt = false;
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
    int numDirtParticles = 0;
    for (int i = 0; i < MAX_PARTICLES; i++)
    {
        if (m_particles[i].alpha > 0.01f)
        {
            numQuads++;
            if (m_particles[i].isDirt)
                numDirtParticles++;
        }
    }

    if (numQuads == 0)
        return;

    uint32_t numVertices = numQuads * 6; // 6 vertices per quad (2 triangles)

    // Check if we can allocate transient buffer
    uint32_t available = bgfx::getAvailTransientVertexBuffer(numVertices, ParticleVertex::ms_layout);
    if (available < numVertices)
        return;

    // Build vertex buffer with particle quads
    bgfx::TransientVertexBuffer tvb;
    bgfx::allocTransientVertexBuffer(&tvb, numVertices, ParticleVertex::ms_layout);

    ParticleVertex* vertices = (ParticleVertex*)tvb.data;
    int vertexIdx = 0;

    const float particleSize = 10.0f; // Very large for testing visibility

    for (int i = 0; i < MAX_PARTICLES; i++)
    {
        const Particle& p = m_particles[i];
        if (p.alpha < 0.01f)
            continue;

        float px = p.x;
        float py = p.y;
        float pz = p.z;
        float size = particleSize;
        float isDirt = p.isDirt ? 1.0f : 0.0f;

        // Create a small quad around the particle center
        // Triangle 1
        vertices[vertexIdx].x = px - size; vertices[vertexIdx].y = py - size; vertices[vertexIdx].z = pz;
        vertices[vertexIdx].alpha = p.alpha;
        vertices[vertexIdx].isDirt = isDirt;
        vertexIdx++;

        vertices[vertexIdx].x = px + size; vertices[vertexIdx].y = py - size; vertices[vertexIdx].z = pz;
        vertices[vertexIdx].alpha = p.alpha;
        vertices[vertexIdx].isDirt = isDirt;
        vertexIdx++;

        vertices[vertexIdx].x = px + size; vertices[vertexIdx].y = py + size; vertices[vertexIdx].z = pz;
        vertices[vertexIdx].alpha = p.alpha;
        vertices[vertexIdx].isDirt = isDirt;
        vertexIdx++;

        // Triangle 2
        vertices[vertexIdx].x = px - size; vertices[vertexIdx].y = py - size; vertices[vertexIdx].z = pz;
        vertices[vertexIdx].alpha = p.alpha;
        vertices[vertexIdx].isDirt = isDirt;
        vertexIdx++;

        vertices[vertexIdx].x = px + size; vertices[vertexIdx].y = py + size; vertices[vertexIdx].z = pz;
        vertices[vertexIdx].alpha = p.alpha;
        vertices[vertexIdx].isDirt = isDirt;
        vertexIdx++;

        vertices[vertexIdx].x = px - size; vertices[vertexIdx].y = py + size; vertices[vertexIdx].z = pz;
        vertices[vertexIdx].alpha = p.alpha;
        vertices[vertexIdx].isDirt = isDirt;
        vertexIdx++;
    }

    // Set state for transparent particles
    // IMPORTANT: For screen-space particles, disable depth testing and depth write
    // Screen-space rendering doesn't use perspective depth
    bgfx::setState(0
        | BGFX_STATE_WRITE_RGB
        | BGFX_STATE_WRITE_A
        | BGFX_STATE_BLEND_ALPHA
        | BGFX_STATE_MSAA
        // NO depth testing for screen-space particles
    );

    bgfx::setVertexBuffer(0, &tvb);

    bgfx::submit(viewId, m_program);
}

void DustParticleSystem::spawnDirtParticles(int worldX, int worldY, int worldZ, int count)
{
    if (!m_initialized)
        return;

    // Find inactive particles and spawn brown dirt particles
    // For simplicity, convert world coords to screen-space using simple mapping
    // In the intro, the car drives from left to right on screen
    // worldX/Z map to screen X, worldY maps to screen Y

    // Simple world-to-screen mapping (approximate for intro scene)
    float screenX = (float)worldX * 0.05f + 160.0f;  // Center around screen middle
    float screenY = 200.0f - (float)worldY * 0.02f;  // Invert Y (world up is screen down)
    float screenZ = (float)worldZ * 0.5f + 600.0f;   // Map to depth range

    int spawned = 0;
    for (int i = 0; i < MAX_PARTICLES && spawned < count; i++)
    {
        Particle& p = m_particles[i];

        // Use particles with low lifetime or far off-screen
        if (p.life < 0.5f || p.y < -50.0f || p.y > 250.0f)
        {
            // Spawn brown dirt particle at car's rear position
            p.x = screenX + randomFloat(-3.0f, 3.0f);
            p.y = screenY + randomFloat(-2.0f, 2.0f);
            p.z = screenZ + randomFloat(-20.0f, 20.0f);

            // Dirt particles kick up and back, then settle down
            p.vx = randomFloat(-20.0f, -5.0f);  // Backwards from car motion
            p.vy = randomFloat(-30.0f, -10.0f); // Upward initially
            p.vz = randomFloat(-5.0f, 5.0f);

            // Brown dirt particles - larger and more visible than dust
            p.size = randomFloat(1.5f, 3.0f);
            p.alpha = randomFloat(0.4f, 0.7f);

            // Short lifetime for dirt puffs
            p.maxLife = randomFloat(0.8f, 1.5f);
            p.life = p.maxLife;

            // Mark as brown dirt
            p.isDirt = true;

            spawned++;
        }
    }
}
