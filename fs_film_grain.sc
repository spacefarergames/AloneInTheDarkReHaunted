$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(s_texColor, 0);
uniform vec4 u_filmGrainParams; // x: intensity, y: time
uniform vec4 u_time;

// Simple noise function
float hash(vec2 p)
{
    return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);
}

void main()
{
    vec4 color = texture2D(s_texColor, v_texcoord0);
    
    // Generate noise based on position and time
    vec2 noiseCoord = v_texcoord0 * 1000.0 + u_time.x * 10.0;
    float noise = hash(noiseCoord) * 2.0 - 1.0;
    
    // Apply film grain
    float intensity = u_filmGrainParams.x;
    color.rgb += noise * intensity;
    
    gl_FragColor = color;
}
