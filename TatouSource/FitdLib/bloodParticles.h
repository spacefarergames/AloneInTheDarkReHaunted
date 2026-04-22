///////////////////////////////////////////////////////////////////////////////
// Blood Particle Effects System
//
// Handles dynamic blood splatter and particle effects when actors take damage.
// Renders blood particles at actor positions with realistic gravity, velocity,
// and fade animations.
//
// Copyright (C) 2026 Infogrames / Spacefarer Retro Remasters LLC
///////////////////////////////////////////////////////////////////////////////

#ifndef BLOOD_PARTICLES_H
#define BLOOD_PARTICLES_H

#include <vector>
#include <chrono>

///////////////////////////////////////////////////////////////////////////////
// Blood Particle Structure
///////////////////////////////////////////////////////////////////////////////

struct BloodParticle
{
    float posX, posY, posZ;      // World position
    float velX, velY, velZ;      // Velocity (world units per second)
    float screenX, screenY;      // Screen space position for rendering
    float lifetime;              // Remaining lifetime in seconds
    float maxLifetime;           // Initial lifetime
    float size;                  // Particle size in pixels
    float alpha;                 // Opacity (0.0 - 1.0)
    
    // Color (darkish red for blood)
    float r, g, b;
    
    // Physics
    static constexpr float GRAVITY = -9.8f;  // World units per second^2
    static constexpr float DRAG = 0.98f;     // Velocity damping per frame
    
    BloodParticle(float x, float y, float z, float vx, float vy, float vz, 
                  float life, float sz);
};

///////////////////////////////////////////////////////////////////////////////
// Blood Particle System
///////////////////////////////////////////////////////////////////////////////

class BloodParticleSystem
{
public:
    static constexpr int MAX_BLOOD_PARTICLES = 200;
    
    struct Splatter
    {
        float posX, posY, posZ;      // Splatter center position
        int particleCount;           // Particles spawned
        float splashRadius;          // Spread radius for burst
    };
    
    BloodParticleSystem();
    ~BloodParticleSystem();
    
    // Spawn a blood splatter burst at given location
    void spawnBloodSplatter(float x, float y, float z, 
                            int particleCount = 8,
                            float velocityScale = 1.0f);
    
    // Spawn a blood spray in a particular direction
    void spawnBloodSpray(float x, float y, float z,
                        float dirX, float dirY, float dirZ,
                        int particleCount = 12,
                        float velocityScale = 1.5f);
    
    // Update all particles (physics, lifetime, screen position)
    void update(float deltaTime);
    
    // Render all particles to screen
    void render();
    
    // Clear all particles
    void clear();
    
private:
    std::vector<BloodParticle> particles;
    float randomFloat(float min, float max);
};

///////////////////////////////////////////////////////////////////////////////
// Global System Functions
///////////////////////////////////////////////////////////////////////////////

void initBloodParticles();
void shutdownBloodParticles();
void updateBloodParticles(float deltaTime);
void renderBloodParticles();

// Spawn blood effect at actor position (called when actor takes damage)
void spawnBloodAtActor(int actorIdx, float damageAmount);

// Get global blood particle system instance
extern BloodParticleSystem* g_bloodParticleSystem;

#endif // BLOOD_PARTICLES_H
