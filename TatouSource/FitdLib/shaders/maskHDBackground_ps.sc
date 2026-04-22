$input v_texcoord0

#include "bgfx_shader.sh"

SAMPLER2D(s_maskPaletteTexture, 0);
SAMPLER2D(s_backgroundTexture, 2);
uniform vec4 u_fadeLevel;

// Lantern lighting uniforms — must match hdBackground_ps.sc exactly
uniform vec4 u_lanternLight;
uniform vec4 u_lanternLightColor;
uniform vec4 u_lanternAmbient;

void main()
{
    // Bilinear-filtered R8 mask: values smoothly interpolate 0.0->1.0 at edges for AA
    float maskAlpha = texture2D(s_maskPaletteTexture, v_texcoord0).r;
    if(maskAlpha < 0.01)
        discard;

    vec4 color = texture2D(s_backgroundTexture, v_texcoord0);
    vec3 rgb = color.rgb;

    // Apply the same luminance-guided lantern lighting as hdBackground_ps.sc
    // so the mask quad matches the background exactly (no seam/mismatch).
    float luminance = dot(rgb, vec3(0.299, 0.587, 0.114));

    float lanternIntensity = u_lanternLight.z;
    float influence        = u_lanternLightColor.w;

    if (lanternIntensity > 0.001 && influence > 0.001)
    {
        vec2  lanternUV   = u_lanternLight.xy;
        float invRadiusSq = u_lanternLight.w;
        vec2  delta       = v_texcoord0 - lanternUV;
        float distSq      = dot(delta, delta);

        float attenuation = clamp(1.0 - distSq * invRadiusSq, 0.0, 1.0);
        attenuation       = attenuation * attenuation;

        float surfaceFactor = mix(0.15, 1.0, luminance);
        float lightAmount   = attenuation * lanternIntensity * influence * surfaceFactor;

        vec3 lanternColor = u_lanternLightColor.xyz;
        rgb = rgb + lanternColor * lightAmount;

        float shadowFactor = u_lanternAmbient.x;
        float farFromLight = 1.0 - attenuation;
        float darkMask     = (1.0 - luminance);
        float darken       = 1.0 - (farFromLight * darkMask * (1.0 - shadowFactor) * influence);
        rgb = rgb * darken;
    }

    rgb = clamp(rgb, vec3_splat(0.0), vec3_splat(1.0));

    // Apply fade level and slight darkening; use maskAlpha for smooth edge blending
    gl_FragColor = vec4(rgb * u_fadeLevel.x * 0.99, maskAlpha);
}