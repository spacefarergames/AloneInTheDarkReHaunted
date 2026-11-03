// Generated from FitdLib/shaders/postprocess_vs.sc and ssao_blur_ps.sc. Regenerate with Unity/tools/convert_shaders.py.
Shader "Hidden/Fitd/postprocess_vs/ssao_blur_ps" {
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

s_texColor("s_texColor", 2D) = "white" {}
s_texDepth("s_texDepth", 2D) = "white" {} }
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
sampler2D s_texColor; float4 s_texColor_TexelSize; float s_texColor_FitdFlipY; float s_texColor_FitdDepthReverse;
float4 sample_s_texColor(float2 uv) { uv.y=lerp(uv.y,1.0-uv.y,s_texColor_FitdFlipY); float4 value=tex2D(s_texColor,uv); value.r=lerp(value.r,1.0-value.r,s_texColor_FitdDepthReverse); return value; }
float4 lod_s_texColor(float2 uv,float lod) { uv.y=lerp(uv.y,1.0-uv.y,s_texColor_FitdFlipY); float4 value=tex2Dlod(s_texColor,float4(uv,0,lod)); value.r=lerp(value.r,1.0-value.r,s_texColor_FitdDepthReverse); return value; }

sampler2D s_texDepth; float4 s_texDepth_TexelSize; float s_texDepth_FitdFlipY; float s_texDepth_FitdDepthReverse;
float4 sample_s_texDepth(float2 uv) { uv.y=lerp(uv.y,1.0-uv.y,s_texDepth_FitdFlipY); float4 value=tex2D(s_texDepth,uv); value.r=lerp(value.r,1.0-value.r,s_texDepth_FitdDepthReverse); return value; }
float4 lod_s_texDepth(float2 uv,float lod) { uv.y=lerp(uv.y,1.0-uv.y,s_texDepth_FitdFlipY); float4 value=tex2Dlod(s_texDepth,float4(uv,0,lod)); value.r=lerp(value.r,1.0-value.r,s_texDepth_FitdDepthReverse); return value; }











   // AO texture to blur
   // Depth for edge-aware blur
float4 u_blurDirection; // xy: blur direction (texel size)
float4 u_depthParams;   // x: near, y: far

float linearizeDepth(float d)
{
    float n = u_depthParams.x;
    float f = u_depthParams.y;
    return n * f / (f - d * (f - n));
}



Varyings vert(AppData IN) { Varyings OUT = (Varyings)0;

    OUT.pos = float4(IN.position.xy, 0.0, 1.0);
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

    float centerAO = sample_s_texColor( IN.uv0).r;
    float centerDepth = linearizeDepth(sample_s_texDepth( IN.uv0).x);

    float2 texelSize = u_blurDirection.xy;

    // Depth-aware 9-tap Gaussian blur
    float weights[5];
    weights[0] = 0.227027;
    weights[1] = 0.1945946;
    weights[2] = 0.1216216;
    weights[3] = 0.054054;
    weights[4] = 0.016216;

    float result = centerAO * weights[0];
    float totalWeight = weights[0];

    for (int i = 1; i < 5; i++)
    {
        float2 offset = texelSize * float(i);

        // Positive direction
        float sAO = lod_s_texColor( IN.uv0 + offset, 0.0).r;
        float sDepth = linearizeDepth(lod_s_texDepth( IN.uv0 + offset, 0.0).x);
        float depthWeight = 1.0 / (1.0 + abs(centerDepth - sDepth) * 0.5);
        float w = weights[i] * depthWeight;
        result += sAO * w;
        totalWeight += w;

        // Negative direction
        sAO = lod_s_texColor( IN.uv0 - offset, 0.0).r;
        sDepth = linearizeDepth(lod_s_texDepth( IN.uv0 - offset, 0.0).x);
        depthWeight = 1.0 / (1.0 + abs(centerDepth - sDepth) * 0.5);
        w = weights[i] * depthWeight;
        result += sAO * w;
        totalWeight += w;
    }

    result /= totalWeight;

    outputColor = float4(result, result, result, 1.0);

return outputColor; }

ENDHLSL
} }
}
