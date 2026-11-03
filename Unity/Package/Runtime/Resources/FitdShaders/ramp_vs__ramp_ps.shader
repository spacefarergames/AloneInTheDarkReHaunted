// Generated from FitdLib/shaders/ramp_vs.sc and ramp_ps.sc. Regenerate with Unity/tools/convert_shaders.py.
Shader "Hidden/Fitd/ramp_vs/ramp_ps" {
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









float4 u_rampAtlasInfo;  // x=atlasWidth, y=atlasHeight, z=reserved, w=reserved


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

// Ramp atlas texture sampler (for textured ramp atlases)


float _mod(float x, float y)
{
  return x - y * floor(x/y);
}



Varyings vert(AppData IN) { Varyings OUT = (Varyings)0;

    OUT.pos = float4(IN.position.x/160.0-1.0, 1.0-IN.position.y/100.0, IN.position.z/40960.f, 1.0);
    OUT.uv0 = IN.uv0;

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

    // Check if we have atlas texture dimensions (non-zero = atlas available)
    if (u_rampAtlasInfo.x > 0.0 && u_rampAtlasInfo.y > 0.0)
    {
        // Sample from ramp atlas texture using UV coordinates from IN.uv0
        float4 texColor = sample_s_modelTexture( IN.uv0);
        outputColor = texColor;
    }
    else
    {
        // Fallback to palette-based rendering (when no ramp atlas available)
        float colorf = IN.uv0.x;

        colorf = _mod(colorf, 2.f);

        if(colorf > 1.f) {
            colorf = 1.f - (colorf - 1.f);
        }

        int color = int(colorf * 15.f);
        int bank = int(IN.uv0.y * 15.f);
        outputColor = getColor(bank, color);
    }
    outputColor = applyModelAmbient(outputColor);

return outputColor; }

ENDHLSL
} }
}
