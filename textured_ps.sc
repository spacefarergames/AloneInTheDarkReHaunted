$input v_texcoord0

#include "bgfx_shader.sh"

SAMPLER2D(s_modelTexture, 0);

void main()
{
    vec4 texColor = texture2D(s_modelTexture, v_texcoord0);
    if (texColor.a < 0.1) discard;
    gl_FragColor = texColor;
}
