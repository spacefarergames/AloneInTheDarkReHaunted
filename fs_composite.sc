$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(s_texColor, 0);   // Main scene
SAMPLER2D(s_texBloom, 1);   // Bloom texture
uniform vec4 u_bloomParams;      // x: threshold, y: intensity
uniform vec4 u_filmGrainParams;  // x: intensity
uniform vec4 u_time;             // x: time in seconds

// Simple noise function for film grain
float hash(vec2 p)
{
    return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);
}

void main()
{
    // Sample main scene
    vec4 color = texture2D(s_texColor, v_texcoord0);
    
    // Add bloom if available
    vec4 bloom = texture2D(s_texBloom, v_texcoord0);
    color.rgb += bloom.rgb * u_bloomParams.y; // bloom intensity
    
    // Add film grain
    vec2 noiseCoord = v_texcoord0 * 1000.0 + u_time.x * 10.0;
    float noise = hash(noiseCoord) * 2.0 - 1.0;
    color.rgb += noise * u_filmGrainParams.x;
    
    gl_FragColor = vec4(color.rgb, 1.0);
}
