$input v_texcoord0

uniform vec4 u_rampAtlasInfo;  // x=atlasWidth, y=atlasHeight, z=reserved, w=reserved

#include "bgfx_shader.sh"
#include "palette.sh"

// Ramp atlas texture sampler (for textured ramp atlases)
SAMPLER2D(s_modelTexture, 0);

float _mod(float x, float y)
{
  return x - y * floor(x/y);
}

void main()
{
    // Check if we have atlas texture dimensions (non-zero = atlas available)
    if (u_rampAtlasInfo.x > 0.0 && u_rampAtlasInfo.y > 0.0)
    {
        // Sample from ramp atlas texture using UV coordinates from v_texcoord0
        vec4 texColor = texture2D(s_modelTexture, v_texcoord0);
        gl_FragColor = texColor;
    }
    else
    {
        // Fallback to palette-based rendering (when no ramp atlas available)
        float colorf = v_texcoord0.x;

        colorf = _mod(colorf, 2.f);

        if(colorf > 1.f) {
            colorf = 1.f - (colorf - 1.f);
        }

        int color = int(colorf * 15.f);
        int bank = int(v_texcoord0.y * 15.f);
        gl_FragColor = getColor(bank, color);
    }
}
