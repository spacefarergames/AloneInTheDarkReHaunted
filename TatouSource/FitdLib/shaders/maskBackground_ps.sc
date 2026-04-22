$input v_texcoord0

#include "bgfx_shader.sh"
#include "palette.sh"

SAMPLER2D(s_maskPaletteTexture, 0);
USAMPLER2D(s_backgroundTexture, 2);

// Lantern lighting uniforms — only active in HD mode (influence == 0 otherwise)
uniform vec4 u_lanternLight;
uniform vec4 u_lanternLightColor;
uniform vec4 u_lanternAmbient;

void main()
{
    // Use float sampler to match R8 texture format (R8U was changed to R8 for HD AA support)
    float maskAlpha = texture2D(s_maskPaletteTexture, v_texcoord0).r;
    if(maskAlpha < 0.5)
        discard;

    vec2 position = vec2(v_texcoord0.xy) * vec2(320, 200);
    uvec4 rawTexel = texelFetch(s_backgroundTexture, ivec2(position), 0);
    vec3 rgb = getColorFromRawOffset(rawTexel.r).rgb;

    // Apply luminance-guided lantern lighting when in HD mode (influence > 0).
    // In non-HD mode the uniforms are never set so influence stays 0 and this block
    // is skipped entirely — no performance cost and no visual change in SD rooms.
    float influence = u_lanternLightColor.w;
    if (influence > 0.001)
    {
        float luminance       = dot(rgb, vec3(0.299, 0.587, 0.114));
        float lanternIntensity = u_lanternLight.z;
        float invRadiusSq     = u_lanternLight.w;
        vec2  delta           = v_texcoord0 - u_lanternLight.xy;
        float distSq          = dot(delta, delta);
        float attenuation     = clamp(1.0 - distSq * invRadiusSq, 0.0, 1.0);
        attenuation           = attenuation * attenuation;
        float surfaceFactor   = mix(0.15, 1.0, luminance);
        float lightAmount     = attenuation * lanternIntensity * influence * surfaceFactor;
        rgb = rgb + u_lanternLightColor.xyz * lightAmount;
        float farFromLight    = 1.0 - attenuation;
        float darkMask        = 1.0 - luminance;
        float darken          = 1.0 - (farFromLight * darkMask * (1.0 - u_lanternAmbient.x) * influence);
        rgb = clamp(rgb * darken, vec3_splat(0.0), vec3_splat(1.0));
    }

    gl_FragColor = vec4(rgb, 1.0);
}
