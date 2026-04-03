$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(s_texColor, 0);   // AO texture to blur
SAMPLER2D(s_texDepth, 1);   // Depth for edge-aware blur
uniform vec4 u_blurDirection; // xy: blur direction (texel size)
uniform vec4 u_depthParams;   // x: near, y: far

float linearizeDepth(float d)
{
    float n = u_depthParams.x;
    float f = u_depthParams.y;
    return n * f / (f - d * (f - n));
}

void main()
{
    float centerAO = texture2D(s_texColor, v_texcoord0).r;
    float centerDepth = linearizeDepth(texture2D(s_texDepth, v_texcoord0).x);

    vec2 texelSize = u_blurDirection.xy;

    // Depth-aware 9-tap Gaussian blur
    float weights[5];
    weights[0] = 0.227027;
    weights[1] = 0.1945946;
    weights[2] = 0.1216216;
    weights[3] = 0.054054;
    weights[4] = 0.016216;

    float result = centerAO * weights[0];
    float totalWeight = weights[0];

    for (int i = 1; i < 5; i++)
    {
        vec2 offset = texelSize * float(i);

        // Positive direction
        float sAO = texture2DLod(s_texColor, v_texcoord0 + offset, 0.0).r;
        float sDepth = linearizeDepth(texture2DLod(s_texDepth, v_texcoord0 + offset, 0.0).x);
        float depthWeight = 1.0 / (1.0 + abs(centerDepth - sDepth) * 0.5);
        float w = weights[i] * depthWeight;
        result += sAO * w;
        totalWeight += w;

        // Negative direction
        sAO = texture2DLod(s_texColor, v_texcoord0 - offset, 0.0).r;
        sDepth = linearizeDepth(texture2DLod(s_texDepth, v_texcoord0 - offset, 0.0).x);
        depthWeight = 1.0 / (1.0 + abs(centerDepth - sDepth) * 0.5);
        w = weights[i] * depthWeight;
        result += sAO * w;
        totalWeight += w;
    }

    result /= totalWeight;

    gl_FragColor = vec4(result, result, result, 1.0);
}
