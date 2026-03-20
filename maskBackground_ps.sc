$input v_texcoord0

#include "bgfx_shader.sh"
#include "palette.sh"

SAMPLER2D(s_maskPaletteTexture, 0);
USAMPLER2D(s_backgroundTexture, 2);

void main()
{
    // Use float sampler to match R8 texture format (R8U was changed to R8 for HD AA support)
    float maskAlpha = texture2D(s_maskPaletteTexture, v_texcoord0).r;
    if(maskAlpha < 0.5)
        discard;

    vec2 position = vec2(v_texcoord0.xy) * vec2(320, 200);
    uvec4 rawTexel = texelFetch(s_backgroundTexture, ivec2(position), 0);
    gl_FragColor = getColorFromRawOffset(rawTexel.r);
}