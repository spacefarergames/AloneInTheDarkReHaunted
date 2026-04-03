$input a_position, a_color0
$output v_color0

#include <bgfx_shader.sh>

void main()
{
    // Screen-space positions pass through with model-view-proj transform
    vec4 pos = vec4(a_position.xyz, 1.0);
    gl_Position = mul(pos, u_modelViewProj);

    // Dust color (warm whitish) with alpha
    v_color0 = vec4(0.95, 0.93, 0.90, a_color0.x);
}
