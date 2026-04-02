$input v_texcoord0, v_sphereParams, v_screenSpacePosition

uniform vec4 u_sphereAtlasInfo;  // x=atlasWidth, y=atlasHeight, z=reserved, w=reserved

#include "bgfx_shader.sh"
#include "palette.sh"

// Sphere atlas texture sampler (for textured sphere atlases)
SAMPLER2D(s_modelTexture, 0);

void main()
{
    vec2 sphereCenter = v_sphereParams.xy;
    float sphereSize = v_sphereParams.z;

    float distanceToCircleOnScanline;
    vec2 normalizedPosition = (v_screenSpacePosition.xy - sphereCenter.xy) / sphereSize;

    float xScalingRatio = 5.f/6.f;

    normalizedPosition.x *= xScalingRatio;
    if(length(normalizedPosition) > 1.f)
        discard;

    int material = int(v_sphereParams.w);

    // Check if we have atlas texture dimensions (non-zero = atlas available)
    if (u_sphereAtlasInfo.x > 0.0 && u_sphereAtlasInfo.y > 0.0)
    {
        // Sample from sphere atlas texture using grid-based UV coordinates
        // v_texcoord0 contains the grid-based UV already computed in CPU
        vec4 texColor = texture2D(s_modelTexture, v_texcoord0);
        gl_FragColor = texColor;

        // Apply transparency for transparent material
        if (material == 2)
        {
            gl_FragColor.w *= 0.5f;
        }
    }
    else
    {
        // Fallback to palette-based rendering (when no sphere atlas available)
        int color = int(v_texcoord0.x * 15.f);
        int bank = int(v_texcoord0.y * 15.f);

        switch(material) {
        case 0: // flat
            gl_FragColor = getColor(bank, color);
            break;
        case 2: //transparent
            gl_FragColor = getColor(bank, color);
            gl_FragColor.w = 0.5f;
            break;
        case 3: //marbre
            float angle = asin(normalizedPosition.y);
            distanceToCircleOnScanline = ((normalizedPosition.x / cos(angle)) / 2.f) + 0.5f;
            color = int((distanceToCircleOnScanline) * 15.f);
            gl_FragColor = getColor(bank, color);
            break;
        default:
            gl_FragColor = getColor(bank, color);
            break;
        }
    }
}
