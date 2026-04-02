$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(s_texColor, 0);   // Main scene
SAMPLER2D(s_texBloom, 1);   // Bloom texture
SAMPLER2D(s_texAO, 2);      // Ambient occlusion
SAMPLER2D(s_texSSGI, 3);    // Screen-Space Global Illumination
uniform vec4 u_bloomParams;      // x: threshold, y: intensity
uniform vec4 u_filmGrainParams;  // x: intensity
uniform vec4 u_ssaoParams;       // x: radius, y: bias, z: intensity (used here for enable flag)
uniform vec4 u_ssgiParams;       // x: radius, y: intensity
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

    // Apply ambient occlusion if intensity > 0
    float aoIntensity = u_ssaoParams.z;
    if (aoIntensity > 0.0)
    {
        float ao = texture2D(s_texAO, v_texcoord0).r;
        color.rgb *= ao;
    }

    // Add SSGI (indirect illumination) if intensity > 0
    float ssgiIntensity = u_ssgiParams.y;
    if (ssgiIntensity > 0.0)
    {
        vec3 ssgi = texture2D(s_texSSGI, v_texcoord0).rgb;
        // Additive blending for indirect light contribution
        color.rgb += ssgi * ssgiIntensity;
    }

    // Add bloom if intensity > 0
    vec4 bloom = texture2D(s_texBloom, v_texcoord0);
    color.rgb += bloom.rgb * u_bloomParams.y;

    // Add film grain if intensity > 0
    float grainIntensity = u_filmGrainParams.x;
    if (grainIntensity > 0.0)
    {
        vec2 noiseCoord = v_texcoord0 * 1000.0 + u_time.x * 10.0;
        float noise = hash(noiseCoord) * 2.0 - 1.0;
        color.rgb += vec3_splat(noise * grainIntensity);
    }

    gl_FragColor = vec4(color.rgb, 1.0);
}
