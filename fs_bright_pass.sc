$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(s_texColor, 0);
uniform vec4 u_bloomParams; // x: threshold, y: intensity

void main()
{
    vec4 color = texture2D(s_texColor, v_texcoord0);
    
    // Calculate luminance
    float luminance = dot(color.rgb, vec3(0.299, 0.587, 0.114));
    
    // Extract bright pixels above threshold
    float threshold = u_bloomParams.x;
    float brightness = max(luminance - threshold, 0.0) / (1.0 - threshold);
    
    gl_FragColor = vec4(color.rgb * brightness, 1.0);
}
