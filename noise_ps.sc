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
    // Standard luminance formula weighted for human eye perception
    float luminance = dot(baseColor.rgb, vec3(0.299, 0.587, 0.114));

    // Threshold for noise application (0.4 = apply noise to colors darker than 40% brightness)
    // Adjust this value to control which areas get noise:
    //   0.3 = only very dark areas (mostly shadows)
    //   0.4 = dark areas (clothing, shadows)
    //   0.5 = medium-dark areas (more aggressive)
    float noiseThreshold = 0.4;

    if (luminance < noiseThreshold)
    {
        // Dark area (clothing/shadows) - apply film grain noise
        uvec2 noiseSampler = uvec2(v_screenSpacePosition.xy * v_screenSpacePosition.z) % uvec2(255, 255);
        float noise = float(texelFetch(s_noiseTexture, ivec2(noiseSampler), 0).r) / 255.f;

        // Scale noise strength based on how dark the area is
        // Darker areas get more pronounced noise
        float noiseStrength = (noiseThreshold - luminance) / noiseThreshold;
        gl_FragColor = baseColor * (0.85f + (noise * 0.15f) * noiseStrength);
    }
    else
    {
        // Bright area (skin, face, hair) - no noise, just flat color
        gl_FragColor = baseColor;
    }
}

