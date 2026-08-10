$input a_position, a_texcoord0, a_color0, a_color1
$output v_color0, v_color1, v_texcoord0

#include <bgfx_shader.sh>

void main()
{
    // Screen-space orthographic projection - match background shader
    // Convert from 320x200 game space to normalized device coordinates (-1 to 1)
    float x = (a_position.x / 160.0) - 1.0;  // 0-320 to -1..1
    float y = 1.0 - (a_position.y / 100.0);  // 0-200 to 1..-1 (inverted)

    // IMPORTANT: Use z=0.0 to match other screen-space overlays
    // Screen-space rendering doesn't need depth; always render on top
    gl_Position = vec4(x, y, 0.0, 1.0);

    // Pass through alpha and isDirt flag to pixel shader
    v_color0 = a_color0;  // alpha
    v_color1 = a_color1;  // isDirt (0.0 = white dust, 1.0 = brown dirt)
    v_texcoord0 = a_texcoord0;
}
