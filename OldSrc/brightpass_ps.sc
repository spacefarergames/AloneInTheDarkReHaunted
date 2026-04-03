$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(s_texColor, 0);
uniform vec4 u_bloomParams; // x: threshold, y: intensity

void main()
{
    vec4 color = texture2D(s_texColor, v_texcoord0);

    // Use max color channel for extraction (catches saturated colors the game uses)
    float brightness = max(color.r, max(color.g, color.b));

    // Soft knee extraction - gradual ramp above threshold
    float threshold = u_bloomParams.x;
    float knee = threshold * 0.5;
    float soft = brightness - threshold + knee;
    soft = clamp(soft / (2.0 * knee + 0.0001), 0.0, 1.0);
    soft = soft * soft;

    float contribution = max(soft, step(threshold, brightness));

    gl_FragColor = vec4(color.rgb * contribution, 1.0);
}
