$input v_texcoord0
#include "bgfx_shader.sh"
uniform vec4 u_contactShadowParams;
void main()
{
    float radiusSq = dot(v_texcoord0, v_texcoord0);
    if (radiusSq >= 1.0) discard;
    float falloff = 1.0 - radiusSq;
    gl_FragColor = vec4(0.0, 0.0, 0.0, u_contactShadowParams.x * falloff * falloff);
}
