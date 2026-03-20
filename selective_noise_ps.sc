$input v_texcoord0, v_screenSpacePosition

#include "bgfx_shader.sh"
#include "palette.sh"

USAMPLER2D(s_noiseTexture, 0);

void main()
{
    int color = int(v_texcoord0.x * 15.f);
    int bank = int(v_texcoord0.y * 15.f);

    vec4 baseColor = getColor(bank, color);
    
    // Calculate luminance to detect dark vs light areas
    float luminance = dot(baseColor.rgb, vec3(0.299, 0.587, 0.114));
    
    // Only apply noise to dark areas (clothing/shadows)
    // Preserve bright areas (skin, face, hair highlights)
    float noiseThreshold = 0.4; // Adjust this value (0.3-0.5 works well)
    
    if (luminance < noiseThreshold)
    {
        // Dark area - apply film grain noise
        uvec2 noiseSampler = uvec2(v_screenSpacePosition.xy * v_screenSpacePosition.z) % uvec2(255, 255);
        float noise = float(texelFetch(s_noiseTexture, ivec2(noiseSampler), 0).r) / 255.f;
        
        // Blend noise based on darkness (darker = more noise)
        float noiseStrength = (noiseThreshold - luminance) / noiseThreshold;
        gl_FragColor = baseColor * (0.5f + noise/2.f * noiseStrength);
    }
    else
    {
        // Bright area (skin/face) - no noise, just flat color
        gl_FragColor = baseColor;
    }
}
