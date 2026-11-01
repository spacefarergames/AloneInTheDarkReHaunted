#include "bgfx_shader.sh"
uniform vec4 u_softShadow;
void main()
{
    gl_FragColor = vec4(0.0, 0.0, 0.0, u_softShadow.z);
}
