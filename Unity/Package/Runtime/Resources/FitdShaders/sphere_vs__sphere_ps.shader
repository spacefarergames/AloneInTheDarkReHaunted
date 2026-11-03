// Generated from FitdLib/shaders/sphere_vs.sc and sphere_ps.sc. Regenerate with Unity/tools/convert_shaders.py.
Shader "Hidden/Fitd/sphere_vs/sphere_ps" {
Properties { 
_SrcBlend("Source blend", Float) = 1
_DstBlend("Destination blend", Float) = 0
_SrcBlendAlpha("Source alpha blend", Float) = 1
_DstBlendAlpha("Destination alpha blend", Float) = 0
_BlendOp("Blend operation", Float) = 0
_BlendOpAlpha("Alpha blend operation", Float) = 0
_ZTest("Depth test", Float) = 8
_ZWrite("Depth write", Float) = 0
_Cull("Cull", Float) = 0
_ColorMask("Color mask", Float) = 15
_StencilRef("Stencil reference", Float) = 0
_StencilMask("Stencil read mask", Float) = 255
_StencilComp("Stencil compare", Float) = 8
_StencilPass("Stencil pass", Float) = 0
_StencilFail("Stencil fail", Float) = 0
_StencilZFail("Stencil depth fail", Float) = 0

s_paletteTexture("s_paletteTexture", 2D) = "white" {}
s_modelTexture("s_modelTexture", 2D) = "white" {} }
SubShader { Tags { "RenderType"="Opaque" }
Pass { 
Cull [_Cull]
ZTest [_ZTest]
ZWrite [_ZWrite]
ColorMask [_ColorMask]
Blend [_SrcBlend] [_DstBlend], [_SrcBlendAlpha] [_DstBlendAlpha]
BlendOp [_BlendOp], [_BlendOpAlpha]
Stencil { Ref [_StencilRef] ReadMask [_StencilMask] Comp [_StencilComp]
Pass [_StencilPass] Fail [_StencilFail] ZFail [_StencilZFail] }

HLSLPROGRAM
#pragma target 3.5
#pragma vertex vert
#pragma fragment frag
#include "UnityCG.cginc"

#define vec2_splat(x) float2(x,x)
#define vec3_splat(x) float3(x,x,x)
#define vec4_splat(x) float4(x,x,x,x)
#define texture2DLod(s,uv,lod) tex2Dlod(s,float4(uv,0,lod))
float4x4 _FitdViewProj;
float4 u_viewRect;
struct AppData {
 float3 position : POSITION; float2 uv0 : TEXCOORD0;
 float4 color0 : COLOR0; float4 uv1 : TEXCOORD1; float4 color1 : TEXCOORD2;
};
struct Varyings {
 float4 pos : SV_POSITION;
 float2 uv0 : TEXCOORD0; float4 sphere : TEXCOORD1; float3 screen : TEXCOORD2;
 float4 color0 : COLOR0; float4 color1 : COLOR1;
};
Texture2D<float4> s_paletteTexture;
uint4 fetch_s_paletteTexture(int2 p,int lod) { return (uint4)round(s_paletteTexture.Load(int3(p,lod))*255.0); }

sampler2D s_modelTexture; float4 s_modelTexture_TexelSize; float s_modelTexture_FitdFlipY; float s_modelTexture_FitdDepthReverse;
float4 sample_s_modelTexture(float2 uv) { uv.y=lerp(uv.y,1.0-uv.y,s_modelTexture_FitdFlipY); float4 value=tex2D(s_modelTexture,uv); value.r=lerp(value.r,1.0-value.r,s_modelTexture_FitdDepthReverse); return value; }
float4 lod_s_modelTexture(float2 uv,float lod) { uv.y=lerp(uv.y,1.0-uv.y,s_modelTexture_FitdFlipY); float4 value=tex2Dlod(s_modelTexture,float4(uv,0,lod)); value.r=lerp(value.r,1.0-value.r,s_modelTexture_FitdDepthReverse); return value; }









float4 u_sphereAtlasInfo;  // x=atlasWidth, y=atlasHeight, z=reserved, w=reserved

// Model lighting uniforms
float4 u_rimLightParams;      // x: intensity, y: reserved, z: reserved, w: reserved
float4 u_rimColor;            // RGB rim light color (xyz), alpha unused
float4 u_specularParams;      // x: specular intensity, y: specular power, z: reserved, w: reserved
float4 u_specularColor;       // RGB specular highlight color (xyz), alpha unused


// w=0 is neutral, including UI, inventory, particles and shadow submissions.
float4 u_modelAmbient;
float4 applyModelAmbient(float4 color)
{
    color.rgb *= lerp(vec3_splat(1.0), u_modelAmbient.rgb, u_modelAmbient.w);
    return color;
}



float4 getColorFromRawOffset(uint colorOffset) {
	float4 outputColor;
	outputColor.r = float(fetch_s_paletteTexture( int2(0, colorOffset), 0).r) / 255.f;
	outputColor.g = float(fetch_s_paletteTexture( int2(1, colorOffset), 0).r) / 255.f;
	outputColor.b = float(fetch_s_paletteTexture( int2(2, colorOffset), 0).r) / 255.f;
	outputColor.w = 1.f;

	return outputColor;
}

float4 getColor(int bank, int color) {
	uint colorOffset = uint((bank << 4) + color);
	return getColorFromRawOffset(colorOffset);
}

// Sphere atlas texture sampler (for textured sphere atlases)




Varyings vert(AppData IN) { Varyings OUT = (Varyings)0;

    OUT.pos = float4(IN.position.x/160.0-1.0, 1.0-IN.position.y/100.0, IN.position.z/40960.f, 1.0);
    OUT.uv0 = IN.uv0;

    float2 sphereCenter = IN.uv1.xy;
    float sphereSize = IN.uv1.z;
    float sphereMaterial = IN.uv1.w;

    OUT.screen = IN.position.xyz;
    OUT.sphere.xy = sphereCenter.xy;
    OUT.sphere.z = sphereSize;
    OUT.sphere.w = sphereMaterial;

#if UNITY_UV_STARTS_AT_TOP
OUT.pos.y = -OUT.pos.y;
#endif
#if defined(UNITY_REVERSED_Z)
OUT.pos.z = OUT.pos.w - OUT.pos.z;
#elif defined(SHADER_API_GLCORE) || defined(SHADER_API_GLES) || defined(SHADER_API_GLES3)
OUT.pos.z = OUT.pos.z * 2.0 - OUT.pos.w;
#endif
return OUT; }

float4 frag(Varyings IN) : SV_Target { float4 outputColor = 0;

    float2 sphereCenter = IN.sphere.xy;
    float sphereSize = IN.sphere.z;

    float distanceToCircleOnScanline;
    float2 normalizedPosition = (IN.screen.xy - sphereCenter.xy) / sphereSize;

    float xScalingRatio = 5.f/6.f;

    normalizedPosition.x *= xScalingRatio;
    float distToCenter = length(normalizedPosition);
    if(distToCenter > 1.f)
        discard;

    int material = int(IN.sphere.w);

    // Check if we have atlas texture dimensions (non-zero = atlas available)
    if (u_sphereAtlasInfo.x > 0.0 && u_sphereAtlasInfo.y > 0.0)
    {
        // Sample from sphere atlas texture using grid-based UV coordinates
        // IN.uv0 contains the grid-based UV already computed in CPU
        float4 texColor = sample_s_modelTexture( IN.uv0);
        outputColor = texColor;

        // Apply transparency for transparent material
        if (material == 2)
        {
            outputColor.w *= 0.5f;
        }
    }
    else
    {
        // Fallback to palette-based rendering (when no sphere atlas available)
        int color = int(IN.uv0.x * 15.f);
        int bank = int(IN.uv0.y * 15.f);

        switch(material) {
        case 0: // flat
            outputColor = getColor(bank, color);
            break;
        case 2: //transparent
            outputColor = getColor(bank, color);
            outputColor.w = 0.5f;
            break;
        case 3: //marbre
            float angle = asin(normalizedPosition.y);
            distanceToCircleOnScanline = ((normalizedPosition.x / cos(angle)) / 2.f) + 0.5f;
            color = int((distanceToCircleOnScanline) * 15.f);
            outputColor = getColor(bank, color);
            break;
        default:
            outputColor = getColor(bank, color);
            break;
        }
    }

    // Apply sphere-specific lighting enhancements
    // Compute approximate sphere normal from normalized position
    float3 sphereNormal = normalize(float3(normalizedPosition.x, normalizedPosition.y, sqrt(max(0.0, 1.0 - distToCenter * distToCenter))));

    // Apply rim lighting: brighten edges where surface normal is perpendicular to view
    if (u_rimLightParams.x > 0.001)
    {
        // View direction approximated as (0, 0, 1) - looking down Z axis
        float rimFactor = 1.0 - abs(sphereNormal.z); // Edge detection
        float3 rimLight = u_rimColor.rgb * u_rimLightParams.x * rimFactor * 0.4;
        outputColor.rgb = min(vec3_splat(1.0), outputColor.rgb + rimLight);
    }

    // Apply specular highlights: bright spot where normal aligns with light direction
    if (u_specularParams.x > 0.001)
    {
        // Simplified specular: light comes from camera direction (0, 0, -1)
        // Specular is brightest where normal points toward camera
        float specularAmount = pow(max(0.0, sphereNormal.z), u_specularParams.y);
        float3 specular = u_specularColor.rgb * u_specularParams.x * specularAmount;
        outputColor.rgb = min(vec3_splat(1.0), outputColor.rgb + specular);
    }
    outputColor = applyModelAmbient(outputColor);

return outputColor; }

ENDHLSL
} }
}
