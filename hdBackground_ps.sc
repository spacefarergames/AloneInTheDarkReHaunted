$input v_texcoord0

#include "bgfx_shader.sh"

SAMPLER2D(s_backgroundTexture, 0);
uniform vec4 u_fadeLevel; // x = fade level (0.0 = black, 1.0 = full brightness)

void main()
{
    // HD backgrounds use RGBA8 format with direct color sampling (no palette lookup)
    vec4 color = texture2D(s_backgroundTexture, v_texcoord0);
    // Apply fade level for fade in/out effects
    gl_FragColor = vec4(color.rgb * u_fadeLevel.x, color.a);
}
