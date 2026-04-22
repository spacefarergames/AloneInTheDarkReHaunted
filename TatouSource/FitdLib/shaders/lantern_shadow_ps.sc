uniform vec4 u_lanternPos;       // xyz = light position (world space), w = distance attenuation
uniform vec4 u_lanternColor;     // xyz = light color, w = intensity
uniform vec4 u_lanternRadius;    // x = radius fraction of screen width (0.0-1.0)

#include <bgfx_shader.sh>

void main()
{
    // Create a radial gradient centred on the actual viewport using bgfx built-in u_viewRect
    // u_viewRect: xy = viewport offset (pixels), zw = viewport size (pixels)
    vec2 screenCenter = u_viewRect.xy + u_viewRect.zw * 0.5;
    vec2 fragPos = gl_FragCoord.xy;

    // Radius in pixels: fraction of viewport width stored in u_lanternRadius.x
    float radiusPx = u_lanternRadius.x * u_viewRect.z;
    float distFromCenter = length(fragPos - screenCenter) / radiusPx;
    float falloff = 1.0 - clamp(distFromCenter, 0.0, 1.0);
    falloff = falloff * falloff;  // Quadratic falloff for realistic decay

    vec3 lightColor = u_lanternColor.xyz;
    float lightIntensity = u_lanternColor.w * falloff * 0.6;

    gl_FragColor = vec4(lightColor, lightIntensity);
}

