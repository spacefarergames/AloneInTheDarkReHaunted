$input v_color0, v_color1

#include <bgfx_shader.sh>

void main()
{
    // Choose color based on particle type
    // v_color1.x == 0.0 -> white dust (atmospheric)
    // v_color1.x == 1.0 -> brown dirt (car particles)

    vec3 dustColor = vec3(0.9, 0.9, 0.85);   // Warm white for dust
    vec3 dirtColor = vec3(0.55, 0.42, 0.30); // Brown/tan for dirt

    vec3 finalColor = mix(dustColor, dirtColor, v_color1.x);
    gl_FragColor = vec4(finalColor, v_color0.x);
}
