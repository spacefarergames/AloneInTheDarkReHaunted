$input v_texcoord0

#include <bgfx_shader.sh>

// Screen-Space Global Illumination (SSGI) Shader
// Approximates indirect lighting by sampling nearby colors based on depth discontinuities

SAMPLER2D(s_texColor, 0);   // Main scene color
SAMPLER2D(s_texDepth, 1);   // Scene depth

uniform vec4 u_ssgiParams;    // x: radius, y: intensity, z: numSamples, w: unused
uniform vec4 u_depthParams;   // x: near, y: far, z: 1/width, w: 1/height
uniform vec4 u_time;          // x: time (for noise variation)

// Linearize depth from [0,1] range to view-space distance
float linearizeDepth(float d)
{
    float n = u_depthParams.x;
    float f = u_depthParams.y;
    return n * f / (f - d * (f - n));
}

// Hash-based pseudo-random for sample rotation
float hash(vec2 p)
{
    return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);
}

vec2 hash2(vec2 p)
{
    vec3 p3 = fract(vec3(p.xyx) * vec3(0.1031, 0.1030, 0.0973));
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.xx + p3.yz) * p3.zy);
}

void main()
{
    float centerDepthRaw = texture2D(s_texDepth, v_texcoord0).x;
    
    // Skip background (depth at or near far plane)
    if (centerDepthRaw >= 0.999)
    {
        gl_FragColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }
    
    float centerDepth = linearizeDepth(centerDepthRaw);
    vec3 centerColor = texture2D(s_texColor, v_texcoord0).rgb;
    
    float radius = u_ssgiParams.x;
    float intensity = u_ssgiParams.y;
    int numSamples = int(u_ssgiParams.z);
    vec2 texelSize = vec2(u_depthParams.z, u_depthParams.w);
    
    // Scale radius by depth (closer objects get larger GI radius in screen space)
    float radiusScreen = radius / centerDepth;
    
    // Pseudo-random rotation angle per pixel to reduce banding
    float angle = hash(v_texcoord0 * 1000.0 + u_time.x) * 6.2831853;
    float cosA = cos(angle);
    float sinA = sin(angle);
    
    // Accumulated indirect lighting
    vec3 indirectLight = vec3(0.0, 0.0, 0.0);
    float totalWeight = 0.0;
    
    // Sample disc pattern for indirect light gathering
    // We sample nearby surfaces and collect their color contribution
    vec2 samples[16];
    samples[ 0] = vec2( 0.355512, -0.709318);
    samples[ 1] = vec2(-0.894878,  0.122717);
    samples[ 2] = vec2( 0.263014,  0.394172);
    samples[ 3] = vec2(-0.366623, -0.674568);
    samples[ 4] = vec2( 0.842890,  0.157139);
    samples[ 5] = vec2(-0.127852,  0.889610);
    samples[ 6] = vec2( 0.693171, -0.526748);
    samples[ 7] = vec2(-0.643845, -0.398119);
    samples[ 8] = vec2( 0.141613,  0.789750);
    samples[ 9] = vec2(-0.786443,  0.568012);
    samples[10] = vec2( 0.500000, -0.114112);
    samples[11] = vec2(-0.321630,  0.213853);
    samples[12] = vec2( 0.125783, -0.457851);
    samples[13] = vec2(-0.581742,  0.789012);
    samples[14] = vec2( 0.912354, -0.312567);
    samples[15] = vec2(-0.234567,  0.123456);
    
    for (int i = 0; i < 16; i++)
    {
        // Skip extra samples beyond numSamples
        float sampleEnabled = (i < numSamples) ? 1.0 : 0.0;

        // Rotate sample
        vec2 s = samples[i];
        vec2 rotated = vec2(s.x * cosA - s.y * sinA, s.x * sinA + s.y * cosA);

        // Scale to screen-space radius
        vec2 sampleUV = v_texcoord0 + rotated * radiusScreen * texelSize * 150.0;

        // Clamp to valid range
        sampleUV = clamp(sampleUV, vec2(0.001, 0.001), vec2(0.999, 0.999));

        // Sample depth and color at this position
        float sampleDepthRaw = texture2DLod(s_texDepth, sampleUV, 0.0).x;
        float sampleDepth = linearizeDepth(sampleDepthRaw);
        vec3 sampleColor = texture2DLod(s_texColor, sampleUV, 0.0).rgb;

        // Calculate depth difference
        float depthDiff = abs(centerDepth - sampleDepth);

        // Weight by depth similarity - samples at similar depth contribute more
        // This ensures we gather light from surfaces that could actually bounce light to us
        float depthWeight = 1.0 / (1.0 + depthDiff * 0.01);

        // Range check: ignore samples too far away in depth
        float rangeCheck = smoothstep(radius * 2.0, 0.0, depthDiff);

        // Distance falloff in screen space
        float dist = length(rotated);
        float distWeight = 1.0 / (1.0 + dist * 2.0);

        // Only gather light from surfaces that are in front (could reflect towards us)
        // A surface slightly in front of us can bounce light to us
        float frontWeight = (sampleDepth < centerDepth + radius * 0.5) ? 1.0 : 0.3;

        // Combine weights (multiply by sampleEnabled to disable extra samples)
        float weight = depthWeight * rangeCheck * distWeight * frontWeight * sampleEnabled;

        // Accumulate colored indirect light
        indirectLight += sampleColor * weight;
        totalWeight += weight;
    }
    
    // Normalize
    if (totalWeight > 0.0)
    {
        indirectLight /= totalWeight;
    }
    
    // Apply intensity and subtle color bleeding
    // We want the indirect light to be subtle and complement the scene
    indirectLight *= intensity;
    
    // Output indirect illumination as RGB
    gl_FragColor = vec4(indirectLight, 1.0);
}
