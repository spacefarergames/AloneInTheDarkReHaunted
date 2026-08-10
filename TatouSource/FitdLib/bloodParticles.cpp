///////////////////////////////////////////////////////////////////////////////
// Blood Particle Effects System - Implementation
//
// Renders dynamic blood particles when actors take damage with realistic
// physics simulation (gravity, drag) and fade animations.
//
// Copyright (C) 2026 Infogrames / Spacefarer Retro Remasters LLC
///////////////////////////////////////////////////////////////////////////////

#include "bloodParticles.h"
#include "common.h"
#include "consoleLog.h"
#include "imguiBGFX.h"
#include "vars.h"
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <algorithm>

// Global blood particle system instance
BloodParticleSystem* g_bloodParticleSystem = nullptr;

static bool g_isMenuActive = false;  // Track if menu is open (don't render blood over UI)

///////////////////////////////////////////////////////////////////////////////
// Blood Particle Implementation
///////////////////////////////////////////////////////////////////////////////

BloodParticle::BloodParticle(float x, float y, float z, float vx, float vy, float vz,
                             float life, float sz)
    : posX(x), posY(y), posZ(z)
    , velX(vx), velY(vy), velZ(vz)
    , lifetime(life), maxLifetime(life)
    , size(sz), alpha(1.0f)
{
    // Dark red blood color with slight variation
    r = 0.6f + (rand() % 10) * 0.01f;  // 0.6 - 0.7
    g = 0.1f;
    b = 0.1f;
}

///////////////////////////////////////////////////////////////////////////////
// Blood Particle System Implementation
///////////////////////////////////////////////////////////////////////////////

BloodParticleSystem::BloodParticleSystem()
{
    particles.reserve(MAX_BLOOD_PARTICLES);
    srand(static_cast<unsigned>(time(nullptr)));
}

BloodParticleSystem::~BloodParticleSystem()
{
    particles.clear();
}

float BloodParticleSystem::randomFloat(float min, float max)
{
    float random = static_cast<float>(rand()) / RAND_MAX;
    return min + random * (max - min);
}

void BloodParticleSystem::spawnBloodSplatter(float x, float y, float z,
                                             int particleCount,
                                             float velocityScale)
{
    // Spawn particles in a spherical burst pattern
    for (int i = 0; i < particleCount; ++i)
    {
        if (particles.size() >= MAX_BLOOD_PARTICLES)
            break;

        // Random direction in 3D sphere
        float theta = randomFloat(0.0f, 2.0f * 3.14159f);
        float phi = randomFloat(0.0f, 3.14159f);

        float speed = randomFloat(50.0f, 150.0f) * velocityScale;

        float velX = speed * sinf(phi) * cosf(theta);
        float velY = speed * cosf(phi) + randomFloat(30.0f, 80.0f);  // Upward bias
        float velZ = speed * sinf(phi) * sinf(theta);

        float lifetime = randomFloat(1.2f, 2.5f);
        float size = randomFloat(1.5f, 3.5f);

        // Small offset from center to spread particles
        float offsetX = randomFloat(-5.0f, 5.0f);
        float offsetY = randomFloat(-3.0f, 3.0f);
        float offsetZ = randomFloat(-5.0f, 5.0f);

        particles.emplace_back(x + offsetX, y + offsetY, z + offsetZ,
                              velX, velY, velZ, lifetime, size);
    }

}

void BloodParticleSystem::spawnBloodSpray(float x, float y, float z,
                                          float dirX, float dirY, float dirZ,
                                          int particleCount,
                                          float velocityScale)
{
    // Normalize direction
    float len = sqrtf(dirX * dirX + dirY * dirY + dirZ * dirZ);
    if (len > 0.0f)
    {
        dirX /= len;
        dirY /= len;
        dirZ /= len;
    }

    // Spawn particles in a cone pattern along direction
    for (int i = 0; i < particleCount; ++i)
    {
        if (particles.size() >= MAX_BLOOD_PARTICLES)
            break;

        // Random angle around the direction vector
        float theta = randomFloat(0.0f, 2.0f * 3.14159f);
        float spreadAngle = randomFloat(0.0f, 0.4f);  // ~23 degree cone

        // Convert to velocity
        float speed = randomFloat(80.0f, 200.0f) * velocityScale;

        // Create perpendicular vector to spread around direction
        float perpX = -dirY;  // Simple perpendicular
        float perpY = dirX;
        float perpZ = 0.0f;

        float len2 = sqrtf(perpX * perpX + perpY * perpY + perpZ * perpZ);
        if (len2 > 0.0f)
        {
            perpX /= len2;
            perpY /= len2;
            perpZ /= len2;
        }

        // Spread velocity
        float cosSpread = cosf(spreadAngle);
        float sinSpread = sinf(spreadAngle);

        float velX = (dirX * cosSpread + perpX * sinSpread) * speed;
        float velY = (dirY * cosSpread) * speed + randomFloat(20.0f, 50.0f);
        float velZ = (dirZ * cosSpread + perpZ * sinSpread) * speed;

        float lifetime = randomFloat(1.0f, 2.0f);
        float size = randomFloat(2.0f, 4.0f);

        float offsetX = randomFloat(-3.0f, 3.0f);
        float offsetY = randomFloat(-2.0f, 2.0f);
        float offsetZ = randomFloat(-3.0f, 3.0f);

        particles.emplace_back(x + offsetX, y + offsetY, z + offsetZ,
                              velX, velY, velZ, lifetime, size);
    }

}

void BloodParticleSystem::update(float deltaTime)
{
    if (deltaTime > 0.1f)
        deltaTime = 0.1f;

    for (BloodParticle& p : particles)
    {
        // Apply gravity
        p.velY += BloodParticle::GRAVITY * deltaTime;

        // Apply drag
        p.velX *= BloodParticle::DRAG;
        p.velY *= BloodParticle::DRAG;
        p.velZ *= BloodParticle::DRAG;

        // Update position
        p.posX += p.velX * deltaTime;
        p.posY += p.velY * deltaTime;
        p.posZ += p.velZ * deltaTime;

        // Update lifetime
        p.lifetime -= deltaTime;

        // Calculate alpha fade (smooth fade out in last 0.3 seconds)
        float fadeTime = 0.3f;
        if (p.lifetime < fadeTime)
        {
            p.alpha = p.lifetime / fadeTime;
        }
        else
        {
            p.alpha = 1.0f;
        }

    }

    particles.erase(
        std::remove_if(particles.begin(), particles.end(),
            [](const BloodParticle& p) { return p.lifetime <= 0.0f; }),
        particles.end());
}

void BloodParticleSystem::render()
{
    if (particles.empty())
        return;

    ImDrawList* drawList = ImGui::GetBackgroundDrawList();
    if (!drawList)
        return;

    ImVec2 displaySize = ImGui::GetIO().DisplaySize;
    if (displaySize.x <= 0.0f || displaySize.y <= 0.0f)
        return;

    float scaleX = displaySize.x / 320.0f;
    float scaleY = displaySize.y / 200.0f;

    extern std::array<tObject, NUM_MAX_OBJECT> ListObjets;

    // Get camera
    s16 heroSlot = currentCameraTargetActor;
    if (heroSlot < 0 || heroSlot >= NUM_MAX_OBJECT)
        heroSlot = 0;

    const tObject& camera = ListObjets[heroSlot];

    // Render each particle
    for (const auto& p : particles)
    {
        // Simple screen projection (would benefit from proper camera matrix)
        // For now, assume orthographic projection centered on camera
        float screenX = (p.posX - camera.worldX) * scaleX + displaySize.x * 0.5f;
        float screenY = (p.posY - camera.worldY) * scaleY + displaySize.y * 0.5f;

        // Clamp to screen bounds (with some margin)
        if (screenX < -50 || screenX > displaySize.x + 50 ||
            screenY < -50 || screenY > displaySize.y + 50)
            continue;

        // Render blood droplet as a small filled circle
        ImU32 bloodColor = ImGui::GetColorU32(ImVec4(p.r, p.g, p.b, p.alpha));

        float particleScreenSize = p.size * 0.5f;  // Scale to screen space
        drawList->AddCircleFilled(ImVec2(screenX, screenY), particleScreenSize, bloodColor, 6);

        // Add slight glow/halo for visual impact
        float haloAlpha = p.alpha * 0.3f;
        ImU32 haloColor = ImGui::GetColorU32(ImVec4(p.r * 1.2f, p.g * 0.8f, p.b * 0.8f, haloAlpha));
        drawList->AddCircle(ImVec2(screenX, screenY), particleScreenSize * 1.5f, haloColor, 8, 0.5f);
    }
}

void BloodParticleSystem::clear()
{
    particles.clear();
}

///////////////////////////////////////////////////////////////////////////////
// Global System Functions
///////////////////////////////////////////////////////////////////////////////

void initBloodParticles()
{
    printf("[BLOOD] Initializing blood particle system\n");
    g_bloodParticleSystem = new BloodParticleSystem();
    printf("[BLOOD] Blood particle system initialized\n");
}

void shutdownBloodParticles()
{
    printf("[BLOOD] Shutting down blood particle system\n");
    if (g_bloodParticleSystem)
    {
        delete g_bloodParticleSystem;
        g_bloodParticleSystem = nullptr;
    }
}

void updateBloodParticles(float deltaTime)
{
    if (!g_bloodParticleSystem)
        return;

    g_bloodParticleSystem->update(deltaTime);
}

void renderBloodParticles()
{
    if (!g_bloodParticleSystem)
        return;

    // Don't render while menus are open
    if (g_isMenuActive)
        return;

    g_bloodParticleSystem->render();
}

void spawnBloodAtActor(int actorIdx, float damageAmount)
{
    if (!g_bloodParticleSystem)
        return;

    extern std::array<tObject, NUM_MAX_OBJECT> ListObjets;

    if (actorIdx < 0 || actorIdx >= NUM_MAX_OBJECT)
        return;

    const tObject& actor = ListObjets[actorIdx];

    // Only spawn blood for animated actors (living characters, not inanimate objects)
    if (!(actor.objectType & AF_ANIMATED))
        return;

    float x = static_cast<float>(actor.worldX);
    float y = static_cast<float>(actor.worldY + 50);  // Slightly above actor center
    float z = static_cast<float>(actor.worldZ);

    // Scale particle count and velocity with damage
    int particleCount = static_cast<int>(4 + damageAmount * 1.5f);
    if (particleCount > 16)
        particleCount = 16;

    float velocityScale = 0.8f + (damageAmount / 100.0f) * 0.4f;  // Scale with damage
    if (velocityScale > 1.5f)
        velocityScale = 1.5f;

    // Spawn splatter
    g_bloodParticleSystem->spawnBloodSplatter(x, y, z, particleCount, velocityScale);
}

// Menu state tracking
void setBloodMenuActive(bool active)
{
    g_isMenuActive = active;
}
