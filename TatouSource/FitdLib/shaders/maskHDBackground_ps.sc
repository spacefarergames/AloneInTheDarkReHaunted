$input v_texcoord0

#include "bgfx_shader.sh"

SAMPLER2D(s_maskPaletteTexture, 0);
SAMPLER2D(s_backgroundTexture, 2);
uniform vec4 u_fadeLevel;

void main()
{
    // Bilinear-filtered R8 mask: values smoothly interpolate 0.0->1.0 at edges for AA
    float maskAlpha = texture2D(s_maskPaletteTexture, v_texcoord0).r;
    if(maskAlpha < 0.01)
        discard;

    vec4 color = texture2D(s_backgroundTexture, v_texcoord0);

    // Apply fade level and slight darkening; use maskAlpha for smooth edge blending
    gl_FragColor = vec4(color.rgb * u_fadeLevel.x * 0.99, maskAlpha);
}