// Generated from FitdLib/shaders/postprocess_vs.sc and composite_ps.sc. Regenerate with Unity/tools/convert_shaders.py.
Shader "Hidden/Fitd/postprocess_vs/composite_ps" {
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
s_texBloom("s_texBloom", 2D) = "white" {}
s_texAO("s_texAO", 2D) = "white" {}
s_texSSGI("s_texSSGI", 2D) = "white" {} }
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

sampler2D s_texBloom; float4 s_texBloom_TexelSize; float s_texBloom_FitdFlipY; float s_texBloom_FitdDepthReverse;
float4 sample_s_texBloom(float2 uv) { uv.y=lerp(uv.y,1.0-uv.y,s_texBloom_FitdFlipY); float4 value=tex2D(s_texBloom,uv); value.r=lerp(value.r,1.0-value.r,s_texBloom_FitdDepthReverse); return value; }
float4 lod_s_texBloom(float2 uv,float lod) { uv.y=lerp(uv.y,1.0-uv.y,s_texBloom_FitdFlipY); float4 value=tex2Dlod(s_texBloom,float4(uv,0,lod)); value.r=lerp(value.r,1.0-value.r,s_texBloom_FitdDepthReverse); return value; }

sampler2D s_texAO; float4 s_texAO_TexelSize; float s_texAO_FitdFlipY; float s_texAO_FitdDepthReverse;
float4 sample_s_texAO(float2 uv) { uv.y=lerp(uv.y,1.0-uv.y,s_texAO_FitdFlipY); float4 value=tex2D(s_texAO,uv); value.r=lerp(value.r,1.0-value.r,s_texAO_FitdDepthReverse); return value; }
float4 lod_s_texAO(float2 uv,float lod) { uv.y=lerp(uv.y,1.0-uv.y,s_texAO_FitdFlipY); float4 value=tex2Dlod(s_texAO,float4(uv,0,lod)); value.r=lerp(value.r,1.0-value.r,s_texAO_FitdDepthReverse); return value; }

sampler2D s_texSSGI; float4 s_texSSGI_TexelSize; float s_texSSGI_FitdFlipY; float s_texSSGI_FitdDepthReverse;
float4 sample_s_texSSGI(float2 uv) { uv.y=lerp(uv.y,1.0-uv.y,s_texSSGI_FitdFlipY); float4 value=tex2D(s_texSSGI,uv); value.r=lerp(value.r,1.0-value.r,s_texSSGI_FitdDepthReverse); return value; }
float4 lod_s_texSSGI(float2 uv,float lod) { uv.y=lerp(uv.y,1.0-uv.y,s_texSSGI_FitdFlipY); float4 value=tex2Dlod(s_texSSGI,float4(uv,0,lod)); value.r=lerp(value.r,1.0-value.r,s_texSSGI_FitdDepthReverse); return value; }










// Integer avalanche hash: no sine approximation or large floating-point
// products that can turn nearby scanlines into correlated moving bands.
// Also compiled directly by the CPU regression test (uint is uint32_t there).
uint pixelNoiseHash(uint x, uint y, uint seed)
{
    uint h = x * 0x9e3779b9u ^ y * 0x85ebca6bu ^ seed * 0xc2b2ae35u;
    h ^= h >> 16;
    h *= 0x7feb352du;
    h ^= h >> 15;
    h *= 0x846ca68bu;
    h ^= h >> 16;
    return h;
}

float pixelNoise(uint x, uint y, uint seed)
{
    // Top 24 bits are exactly representable in a shader float, in [0, 1).
    return float(pixelNoiseHash(x, y, seed) >> 8) * (1.0 / 16777216.0);
}


   // Main scene
   // Bloom texture
      // Ambient occlusion
    // Screen-Space Global Illumination
float4 u_bloomParams;      // x: threshold, y: intensity
float4 u_filmGrainParams;  // x: intensity
float4 u_ssaoParams;       // x: radius, y: bias, z: intensity (used here for enable flag)
float4 u_ssgiParams;       // x: radius, y: intensity
float4 u_time;             // x: time in seconds
float4 u_colorGradeParams; // x: exposure, y: contrast, z: saturation, w: temperature
float4 u_toneParams;       // x: shadow lift, y: highlight rolloff


float luminance(float3 c)
{
    return dot(c, float3(0.2126, 0.7152, 0.0722));
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

    // Sample main scene
    float4 color = sample_s_texColor( IN.uv0);

    // Apply ambient occlusion if intensity > 0
    float aoIntensity = u_ssaoParams.z;
    if (aoIntensity > 0.0)
    {
        float ao = sample_s_texAO( IN.uv0).r;
        color.rgb *= ao;
    }

    // Add SSGI (indirect illumination) if intensity > 0
    float ssgiIntensity = u_ssgiParams.y;
    if (ssgiIntensity > 0.0)
    {
        float3 ssgi = sample_s_texSSGI( IN.uv0).rgb;
        // Additive blending for indirect light contribution
        color.rgb += ssgi * ssgiIntensity;
    }

    // Add bloom if intensity > 0
    float4 bloom = sample_s_texBloom( IN.uv0);
    color.rgb += bloom.rgb * u_bloomParams.y;

    // Scene-referred cinematic grade. Exposure happens before highlight
    // compression so bright windows and lamps retain shape instead of clipping.
    color.rgb *= exp2(u_colorGradeParams.x);
    color.rgb += vec3_splat(u_toneParams.x) * (vec3_splat(1.0) - smoothstep(vec3_splat(0.0), vec3_splat(0.35), color.rgb));
    color.rgb = color.rgb / (vec3_splat(1.0) + color.rgb * u_toneParams.y);

    // A small temperature offset uses opposing red/blue changes and leaves
    // green stable, preserving neutral UI text better than a blanket tint.
    float temperature = u_colorGradeParams.w;
    color.rgb *= float3(1.0 + temperature * 0.10, 1.0, 1.0 - temperature * 0.10);

    float luma = luminance(color.rgb);
    color.rgb = lerp(vec3_splat(luma), color.rgb, u_colorGradeParams.z);
    color.rgb = (color.rgb - vec3_splat(0.5)) * u_colorGradeParams.y + vec3_splat(0.5);

    // Add luminance-adaptive, zero-mean grain. Two decorrelated samples form
    // triangular noise, avoiding the coarse sparkling of white noise.
    float grainIntensity = u_filmGrainParams.x;
    if (grainIntensity > 0.0)
    {
        // Native framebuffer pixels; reseed each grain frame, never scroll UVs.
        uint2 pixel = uint2(floor(IN.pos.xy));
        uint frame = uint(floor(u_time.x * 24.0));
        float noise = pixelNoise(pixel.x, pixel.y, frame)
                    + pixelNoise(pixel.x, pixel.y, frame ^ 0x68bc21ebu) - 1.0;
        float grainMask = 0.35 + 0.65 * (1.0 - smoothstep(0.08, 0.85, luminance(color.rgb)));
        color.rgb += vec3_splat(noise * grainIntensity * grainMask);
    }

    outputColor = float4(clamp(color.rgb, 0.0, 1.0), 1.0);

return outputColor; }

ENDHLSL
} }
}
