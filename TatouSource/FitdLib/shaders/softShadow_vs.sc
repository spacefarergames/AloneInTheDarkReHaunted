$input a_position
#include "bgfx_shader.sh"
uniform vec4 u_softShadow;
void main()
{
    vec2 position = a_position.xy + u_softShadow.xy;
    gl_Position = vec4(position.x / 160.0 - 1.0, 1.0 - position.y / 100.0, a_position.z / 40960.0, 1.0);
}
