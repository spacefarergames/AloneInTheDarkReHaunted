// Generated from FitdLib/shaders/postprocess_vs.sc and ssgi_ps.sc. Regenerate with Unity/tools/convert_shaders.py.
Shader "Hidden/Fitd/postprocess_vs/ssgi_ps" {
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


// Screen-Space Global Illumination (SSGI) Shader
// Approximates indirect lighting by sampling nearby colors based on depth discontinuities

   // Main scene color
   // Scene depth

float4 u_ssgiParams;    // x: radius, y: intensity, z: numSamples, w: unused
float4 u_depthParams;   // x: near, y: far, z: 1/width, w: 1/height

// Linearize depth from [0,1] range to view-space distance
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

    float centerDepthRaw = sample_s_texDepth( IN.uv0).x;
    
    // Skip background (depth at or near far plane)
    if (centerDepthRaw >= 0.999)
    {
        outputColor = float4(0.0, 0.0, 0.0, 1.0);
        return outputColor;
    }
    
    float centerDepth = linearizeDepth(centerDepthRaw);
    float3 centerColor = sample_s_texColor( IN.uv0).rgb;
    
    float radius = u_ssgiParams.x;
    float intensity = u_ssgiParams.y;
    int numSamples = int(u_ssgiParams.z);
    float2 texelSize = float2(u_depthParams.z, u_depthParams.w);
    
    // Scale radius by depth (closer objects get larger GI radius in screen space)
    float radiusScreen = radius / centerDepth;
    
    // Stable per-pixel rotation. With no temporal accumulation, animating
    // this pattern makes stationary surfaces crawl and flicker.
    uint2 pixel = uint2(floor(IN.pos.xy));
    float angle = pixelNoise(pixel.x, pixel.y, 0u) * 6.2831853;
    float cosA = cos(angle);
    float sinA = sin(angle);
    
    // Accumulated indirect lighting
    float3 indirectLight = float3(0.0, 0.0, 0.0);
    float totalWeight = 0.0;
    
    // Sample disc pattern for indirect light gathering
    // We sample nearby surfaces and collect their color contribution
    float2 samples[16];
    samples[ 0] = float2( 0.355512, -0.709318);
    samples[ 1] = float2(-0.894878,  0.122717);
    samples[ 2] = float2( 0.263014,  0.394172);
    samples[ 3] = float2(-0.366623, -0.674568);
    samples[ 4] = float2( 0.842890,  0.157139);
    samples[ 5] = float2(-0.127852,  0.889610);
    samples[ 6] = float2( 0.693171, -0.526748);
    samples[ 7] = float2(-0.643845, -0.398119);
    samples[ 8] = float2( 0.141613,  0.789750);
    samples[ 9] = float2(-0.786443,  0.568012);
    samples[10] = float2( 0.500000, -0.114112);
    samples[11] = float2(-0.321630,  0.213853);
    samples[12] = float2( 0.125783, -0.457851);
    samples[13] = float2(-0.581742,  0.789012);
    samples[14] = float2( 0.912354, -0.312567);
    samples[15] = float2(-0.234567,  0.123456);
    
    for (int i = 0; i < 16; i++)
    {
        // Skip extra samples beyond numSamples
        float sampleEnabled = (i < numSamples) ? 1.0 : 0.0;

        // Rotate sample
        float2 s = samples[i];
        float2 rotated = float2(s.x * cosA - s.y * sinA, s.x * sinA + s.y * cosA);

        // Scale to screen-space radius
        float2 sampleUV = IN.uv0 + rotated * radiusScreen * texelSize * 150.0;

        // Clamp to valid range
        sampleUV = clamp(sampleUV, float2(0.001, 0.001), float2(0.999, 0.999));

        // Sample depth and color at this position
        float sampleDepthRaw = lod_s_texDepth( sampleUV, 0.0).x;
        float sampleDepth = linearizeDepth(sampleDepthRaw);
        float3 sampleColor = lod_s_texColor( sampleUV, 0.0).rgb;

        // Calculate depth difference
        float depthDiff = abs(centerDepth - sampleDepth);

        // Weight by depth similarity - samples at similar depth contribute more
        // This ensures we gather light from surfaces that could actually bounce light to us
        float depthWeight = 1.0 / (1.0 + depthDiff * 0.01);

        // Range check: ignore samples too far away in depth
        float rangeCheck = smoothstep(radius * 2.0, 0.0, depthDiff);

        // Distance falloff in screen space
        float dist = length(rotated);
        float distWeight = 1.0 / (1.0 + dist * 2.0);

        // Only gather light from surfaces that are in front (could reflect towards us)
        // A surface slightly in front of us can bounce light to us
        float frontWeight = (sampleDepth < centerDepth + radius * 0.5) ? 1.0 : 0.3;

        // Combine weights (multiply by sampleEnabled to disable extra samples)
        float weight = depthWeight * rangeCheck * distWeight * frontWeight * sampleEnabled;

        // Accumulate colored indirect light
        indirectLight += sampleColor * weight;
        totalWeight += weight;
    }
    
    // Normalize
    if (totalWeight > 0.0)
    {
        indirectLight /= totalWeight;
    }
    
    // Apply intensity and subtle color bleeding
    // We want the indirect light to be subtle and complement the scene
    indirectLight *= intensity;
    
    // Output indirect illumination as RGB
    outputColor = float4(indirectLight, 1.0);

return outputColor; }

ENDHLSL
} }
}
