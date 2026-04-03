$input v_texcoord0

#include <bgfx_shader.sh>

// SSGI Bilateral Blur Shader
// Smooths the SSGI contribution while preserving depth edges

SAMPLER2D(s_texColor, 0);   // SSGI texture to blur
SAMPLER2D(s_texDepth, 1);   // Scene depth for edge preservation

uniform vec4 u_blurDirection;  // x: 1/width, y: 1/height, z: unused, w: unused
uniform vec4 u_depthParams;    // x: near, y: far, z: 1/width, w: 1/height

// Linearize depth
float linearizeDepth(float d)
{
    float n = u_depthParams.x;
    float f = u_depthParams.y;
    return n * f / (f - d * (f - n));
}

void main()
{
    vec3 centerSSGI = texture2D(s_texColor, v_texcoord0).rgb;
    float centerDepthRaw = texture2D(s_texDepth, v_texcoord0).x;
    float centerDepth = linearizeDepth(centerDepthRaw);
    
    // Skip background
    if (centerDepthRaw >= 0.999)
    {
        gl_FragColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }
    
    vec2 texelSize = vec2(u_blurDirection.x, u_blurDirection.y);
    
    // 9-tap bilateral blur kernel
    vec3 result = centerSSGI * 4.0;
    float totalWeight = 4.0;
    
    // Sample offsets for a 3x3 kernel (excluding center)
    vec2 offsets[8];
    offsets[0] = vec2(-1.0, -1.0);
    offsets[1] = vec2( 0.0, -1.0);
    offsets[2] = vec2( 1.0, -1.0);
    offsets[3] = vec2(-1.0,  0.0);
    offsets[4] = vec2( 1.0,  0.0);
    offsets[5] = vec2(-1.0,  1.0);
    offsets[6] = vec2( 0.0,  1.0);
    offsets[7] = vec2( 1.0,  1.0);
    
    // Corner samples have lower weight in Gaussian blur
    float weights[8];
    weights[0] = 1.0; // corners
    weights[1] = 2.0; // edges
    weights[2] = 1.0;
    weights[3] = 2.0;
    weights[4] = 2.0;
    weights[5] = 1.0;
    weights[6] = 2.0;
    weights[7] = 1.0;
    
    for (int i = 0; i < 8; i++)
    {
        vec2 sampleUV = v_texcoord0 + offsets[i] * texelSize * 1.5;
        sampleUV = clamp(sampleUV, vec2(0.001, 0.001), vec2(0.999, 0.999));

        vec3 sampleSSGI = texture2DLod(s_texColor, sampleUV, 0.0).rgb;
        float sampleDepthRaw = texture2DLod(s_texDepth, sampleUV, 0.0).x;
        float sampleDepth = linearizeDepth(sampleDepthRaw);

        // Depth similarity weight (bilateral filter)
        float depthDiff = abs(centerDepth - sampleDepth);
        float depthWeight = exp(-depthDiff * depthDiff * 0.0001);

        // Combined weight
        float weight = weights[i] * depthWeight;

        result += sampleSSGI * weight;
        totalWeight += weight;
    }
    
    result /= totalWeight;
    
    gl_FragColor = vec4(result, 1.0);
}
