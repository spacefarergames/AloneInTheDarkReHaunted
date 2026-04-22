$input a_position

#include <bgfx_shader.sh>

void main()
{
    // Standard screen-space position
    gl_Position = vec4(a_position, 1.0);
}

