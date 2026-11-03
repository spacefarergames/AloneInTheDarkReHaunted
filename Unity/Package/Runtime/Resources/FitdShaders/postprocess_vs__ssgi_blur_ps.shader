// Generated from FitdLib/shaders/postprocess_vs.sc and ssgi_blur_ps.sc. Regenerate with Unity/tools/convert_shaders.py.
Shader "Hidden/Fitd/postprocess_vs/ssgi_blur_ps" {
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











// SSGI Bilateral Blur Shader
// Smooths the SSGI contribution while preserving depth edges

   // SSGI texture to blur
   // Scene depth for edge preservation

float4 u_blurDirection;  // x: 1/width, y: 1/height, z: unused, w: unused
float4 u_depthParams;    // x: near, y: far, z: 1/width, w: 1/height

// Linearize depth
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

    float3 centerSSGI = sample_s_texColor( IN.uv0).rgb;
    float centerDepthRaw = sample_s_texDepth( IN.uv0).x;
    float centerDepth = linearizeDepth(centerDepthRaw);
    
    // Skip background
    if (centerDepthRaw >= 0.999)
    {
        outputColor = float4(0.0, 0.0, 0.0, 1.0);
        return outputColor;
    }
    
    float2 texelSize = float2(u_blurDirection.x, u_blurDirection.y);
    
    // 9-tap bilateral blur kernel
    float3 result = centerSSGI * 4.0;
    float totalWeight = 4.0;
    
    // Sample offsets for a 3x3 kernel (excluding center)
    float2 offsets[8];
    offsets[0] = float2(-1.0, -1.0);
    offsets[1] = float2( 0.0, -1.0);
    offsets[2] = float2( 1.0, -1.0);
    offsets[3] = float2(-1.0,  0.0);
    offsets[4] = float2( 1.0,  0.0);
    offsets[5] = float2(-1.0,  1.0);
    offsets[6] = float2( 0.0,  1.0);
    offsets[7] = float2( 1.0,  1.0);
    
    // Corner samples have lower weight in Gaussian blur
    float weights[8];
    weights[0] = 1.0; // corners
    weights[1] = 2.0; // edges
    weights[2] = 1.0;
    weights[3] = 2.0;
    weights[4] = 2.0;
    weights[5] = 1.0;
    weights[6] = 2.0;
    weights[7] = 1.0;
    
    for (int i = 0; i < 8; i++)
    {
        float2 sampleUV = IN.uv0 + offsets[i] * texelSize * 1.5;
        sampleUV = clamp(sampleUV, float2(0.001, 0.001), float2(0.999, 0.999));

        float3 sampleSSGI = lod_s_texColor( sampleUV, 0.0).rgb;
        float sampleDepthRaw = lod_s_texDepth( sampleUV, 0.0).x;
        float sampleDepth = linearizeDepth(sampleDepthRaw);

        // Depth similarity weight (bilateral filter)
        float depthDiff = abs(centerDepth - sampleDepth);
        float depthWeight = exp(-depthDiff * depthDiff * 0.0001);

        // Combined weight
        float weight = weights[i] * depthWeight;

        result += sampleSSGI * weight;
        totalWeight += weight;
    }
    
    result /= totalWeight;
    
    outputColor = float4(result, 1.0);

return outputColor; }

ENDHLSL
} }
}
