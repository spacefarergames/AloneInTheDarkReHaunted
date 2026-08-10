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
uniform vec4 u_colorGradeParams; // x: exposure, y: contrast, z: saturation, w: temperature
uniform vec4 u_toneParams;       // x: shadow lift, y: highlight rolloff

// Simple noise function for film grain
float hash(vec2 p)
{
    return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);
}

float luminance(vec3 c)
{
    return dot(c, vec3(0.2126, 0.7152, 0.0722));
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

    // Scene-referred cinematic grade. Exposure happens before highlight
    // compression so bright windows and lamps retain shape instead of clipping.
    color.rgb *= exp2(u_colorGradeParams.x);
    color.rgb += vec3_splat(u_toneParams.x) * (vec3_splat(1.0) - smoothstep(vec3_splat(0.0), vec3_splat(0.35), color.rgb));
    color.rgb = color.rgb / (vec3_splat(1.0) + color.rgb * u_toneParams.y);

    // A small temperature offset uses opposing red/blue changes and leaves
    // green stable, preserving neutral UI text better than a blanket tint.
    float temperature = u_colorGradeParams.w;
    color.rgb *= vec3(1.0 + temperature * 0.10, 1.0, 1.0 - temperature * 0.10);

    float luma = luminance(color.rgb);
    color.rgb = mix(vec3_splat(luma), color.rgb, u_colorGradeParams.z);
    color.rgb = (color.rgb - vec3_splat(0.5)) * u_colorGradeParams.y + vec3_splat(0.5);

    // Add luminance-adaptive, zero-mean grain. Two decorrelated samples form
    // triangular noise, avoiding the coarse sparkling of uniform white noise.
    float grainIntensity = u_filmGrainParams.x;
    if (grainIntensity > 0.0)
    {
        vec2 pixelCoord = v_texcoord0 * vec2(1920.0, 1080.0);
        float frame = floor(u_time.x * 24.0);
        float noise = hash(pixelCoord + frame * 17.0) + hash(pixelCoord * 0.73 - frame * 11.0) - 1.0;
        float grainMask = 0.35 + 0.65 * (1.0 - smoothstep(0.08, 0.85, luminance(color.rgb)));
        color.rgb += vec3_splat(noise * grainIntensity * grainMask);
    }

    gl_FragColor = vec4(clamp(color.rgb, 0.0, 1.0), 1.0);
}
