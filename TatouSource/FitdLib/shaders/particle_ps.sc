$input v_color0, v_color1, v_texcoord0

#include <bgfx_shader.sh>

void main()
{
    // Choose color based on particle type
    // v_color1.x == 0.0 -> white dust (atmospheric)
    // v_color1.x == 1.0 -> brown dirt (car particles)

    vec3 dustColor = vec3(0.9, 0.9, 0.85);   // Warm white for dust
    vec3 dirtColor = vec3(0.55, 0.42, 0.30); // Brown/tan for dirt

    vec2 centered = v_texcoord0 * 2.0 - 1.0;
    float radius2 = dot(centered, centered);
    float softShape = 1.0 - smoothstep(0.12, 1.0, radius2);
    // Dirt is denser in the core; airborne dust stays broad and translucent.
    float core = mix(0.55 + softShape * 0.45, softShape, v_color1.x);
    float alpha = v_color0.x * softShape * core;
    if (alpha < 0.004)
        discard;

    vec3 finalColor = mix(dustColor, dirtColor, v_color1.x);
    finalColor *= 0.82 + softShape * 0.18;
    gl_FragColor = vec4(finalColor, alpha);
}
