$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(s_texColor, 0);
uniform vec4 u_blurDirection; // xy: blur direction, z: blur size

void main()
{
    vec2 texelSize = vec2(1.0 / 1280.0, 1.0 / 720.0); // Will be set via uniform in real implementation
    vec2 blurDir = u_blurDirection.xy;
    float blurSize = u_blurDirection.z;
    
    // Gaussian blur weights (5-tap)
    float weights[5];
    weights[0] = 0.227027;
    weights[1] = 0.1945946;
    weights[2] = 0.1216216;
    weights[3] = 0.054054;
    weights[4] = 0.016216;
    
    vec3 result = texture2D(s_texColor, v_texcoord0).rgb * weights[0];
    
    for(int i = 1; i < 5; i++)
    {
        vec2 offset = blurDir * float(i) * texelSize * blurSize;
        result += texture2D(s_texColor, v_texcoord0 + offset).rgb * weights[i];
        result += texture2D(s_texColor, v_texcoord0 - offset).rgb * weights[i];
    }
    
    gl_FragColor = vec4(result, 1.0);
}
