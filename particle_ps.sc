$input v_color0

#include <bgfx_shader.sh>

void main()
{
    // Simple white dust particle with alpha
    gl_FragColor = vec4(0.9, 0.9, 0.85, v_color0.a);
}
