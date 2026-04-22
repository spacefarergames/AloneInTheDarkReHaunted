uniform vec4 u_lanternPos;       // xyz = position, w = unused
uniform vec4 u_lanternColor;     // xyz = color, w = intensity
uniform vec4 u_lanternIntensity; // x = intensity multiplier
uniform vec4 u_lanternRadius;    // x = light radius

#include <bgfx_shader.sh>

void main()
{
    // Simple bloom - output glow color multiplied by intensity
    vec3 glowColor = u_lanternColor.xyz;
    float glowIntensity = u_lanternColor.w * u_lanternIntensity.x;

    gl_FragColor = vec4(glowColor, glowIntensity);
}

