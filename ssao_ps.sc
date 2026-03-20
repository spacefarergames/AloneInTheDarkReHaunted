$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(s_texDepth, 0);
uniform vec4 u_ssaoParams;     // x: radius, y: bias, z: intensity, w: unused
uniform vec4 u_depthParams;    // x: near, y: far, z: 1/width, w: 1/height
uniform vec4 u_time;           // x: time (for noise rotation)

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

void main()
{
    float depth = texture2D(s_texDepth, v_texcoord0).x;

    // Skip background (depth at or near far plane)
    if (depth >= 0.999)
    {
        gl_FragColor = vec4(1.0, 1.0, 1.0, 1.0);
        return;
    }

    float centerDepth = linearizeDepth(depth);
    float radius = u_ssaoParams.x;
    float bias = u_ssaoParams.y;
    float intensity = u_ssaoParams.z;
    vec2 texelSize = vec2(u_depthParams.z, u_depthParams.w);

    // Scale radius by depth (closer objects get larger AO radius in screen space)
    float radiusScreen = radius / centerDepth;

    // Pseudo-random rotation angle per pixel to reduce banding
    float angle = hash(v_texcoord0 * 1000.0 + u_time.x) * 6.2831853;
    float cosA = cos(angle);
    float sinA = sin(angle);

    // 12-sample disc pattern
    float occlusion = 0.0;
    int numSamples = 12;

    // Fixed sample offsets in a disc pattern
    vec2 samples[12];
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

    for (int i = 0; i < 12; i++)
    {
        // Rotate sample
        vec2 s = samples[i];
        vec2 rotated = vec2(s.x * cosA - s.y * sinA, s.x * sinA + s.y * cosA);

        // Scale to screen-space radius
        vec2 sampleUV = v_texcoord0 + rotated * radiusScreen * texelSize * 200.0;

        // Clamp to valid range
        sampleUV = clamp(sampleUV, vec2(0.001, 0.001), vec2(0.999, 0.999));

        float sampleDepthRaw = texture2DLod(s_texDepth, sampleUV, 0.0).x;
        float sampleDepth = linearizeDepth(sampleDepthRaw);

        // Compare depths: if sample is closer, it occludes the center
        float depthDiff = centerDepth - sampleDepth;

        // Range check: ignore samples too far away (prevents halo artifacts)
        float rangeCheck = smoothstep(0.0, 1.0, radius * 2.0 / (abs(depthDiff) + 0.001));

        // Occlusion: sample is occluding if it's closer (positive depthDiff) and above bias
        occlusion += step(bias, depthDiff) * rangeCheck;
    }

    occlusion = 1.0 - (occlusion / float(numSamples)) * intensity;
    occlusion = clamp(occlusion, 0.0, 1.0);

    gl_FragColor = vec4(occlusion, occlusion, occlusion, 1.0);
}
