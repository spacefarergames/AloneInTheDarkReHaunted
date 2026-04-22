$input v_texcoord0

#include "bgfx_shader.sh"

SAMPLER2D(s_backgroundTexture, 0);
uniform vec4 u_fadeLevel;       // x = fade level (0.0 = black, 1.0 = full brightness)

// Lantern lighting uniforms
// u_lanternLight:      xy = screen-space UV position (0..1), z = effective intensity (0..1), w = 1/radius^2
// u_lanternLightColor: xyz = light color (warm orange), w = backgroundLightInfluence (0..1)
// u_lanternAmbient:    x = ambient darkness multiplier for unlit areas (0..1, 1 = no effect)
uniform vec4 u_lanternLight;
uniform vec4 u_lanternLightColor;
uniform vec4 u_lanternAmbient;

void main()
{
    // HD backgrounds use RGBA8 format with direct color sampling (no palette lookup)
    vec4 color = texture2D(s_backgroundTexture, v_texcoord0);
    vec3 rgb = color.rgb;

    // --- Luminance-guided real-time lantern lighting ---
    // Compute perceptual luminance of the background pixel.
    // This acts as our surface mask: bright areas (lit walls, floors) reflect lantern light
    // more readily; dark areas (shadow corners, recesses) absorb it and stay dark.
    float luminance = dot(rgb, vec3(0.299, 0.587, 0.114));

    float lanternIntensity  = u_lanternLight.z;       // 0..1
    float influence         = u_lanternLightColor.w;  // backgroundLightInfluence 0..1

    if (lanternIntensity > 0.001 && influence > 0.001)
    {
        // Distance from this pixel to the lantern screen position
        vec2  lanternUV   = u_lanternLight.xy;
        float invRadiusSq = u_lanternLight.w;         // 1 / radius^2 (in UV space)
        vec2  delta       = v_texcoord0 - lanternUV;
        float distSq      = dot(delta, delta);

        // Smooth quadratic attenuation — falls off naturally like a real point light
        float attenuation = clamp(1.0 - distSq * invRadiusSq, 0.0, 1.0);
        attenuation       = attenuation * attenuation; // squared for softer roll-off

        // Luminance-based surface factor:
        //   - Already-bright pixels (luminance ~1) pick up full lantern color
        //   - Dark pixels (luminance ~0) receive a smaller fraction — they're in shadow,
        //     the lantern light can't fully reach them, but the warm glow still bleeds in slightly
        float surfaceFactor = mix(0.15, 1.0, luminance);

        // Combined light contribution
        float lightAmount = attenuation * lanternIntensity * influence * surfaceFactor;

        vec3 lanternColor = u_lanternLightColor.xyz;

        // Additive warm light on bright surfaces; also slightly warm-shifts dark areas
        rgb = rgb + lanternColor * lightAmount;

        // Shadow deepening in unlit areas far from lantern:
        // Dark pixels that are far from the light source get pushed darker — they are true
        // shadow volumes now that the lantern is the dominant light in the room.
        float shadowFactor = u_lanternAmbient.x; // 1.0 = no darkening, < 1.0 = deepen shadows
        float farFromLight  = 1.0 - attenuation;
        float darkMask      = (1.0 - luminance);  // Only affects already-dark areas
        float darken        = 1.0 - (farFromLight * darkMask * (1.0 - shadowFactor) * influence);
        rgb = rgb * darken;
    }

    // Clamp to valid range
    rgb = clamp(rgb, vec3_splat(0.0), vec3_splat(1.0));

    // Apply fade level for fade in/out effects
    gl_FragColor = vec4(rgb * u_fadeLevel.x, color.a);
}
