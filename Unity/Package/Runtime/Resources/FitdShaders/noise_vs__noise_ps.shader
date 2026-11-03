// Generated from FitdLib/shaders/noise_vs.sc and noise_ps.sc. Regenerate with Unity/tools/convert_shaders.py.
Shader "Hidden/Fitd/noise_vs/noise_ps" {
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
s_noiseTexture("s_noiseTexture", 2D) = "white" {} }
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

Texture2D<float4> s_noiseTexture;
uint4 fetch_s_noiseTexture(int2 p,int lod) { return (uint4)round(s_noiseTexture.Load(int3(p,lod))*255.0); }










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






Varyings vert(AppData IN) { Varyings OUT = (Varyings)0;

    OUT.pos = float4(IN.position.x/160.0-1.0, 1.0-IN.position.y/100.0, IN.position.z/40960.f, 1.0);
    OUT.uv0 = IN.uv0;
    OUT.screen = IN.position.xyz;

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

    int color = int(IN.uv0.x * 15.f);
    int bank = int(IN.uv0.y * 15.f);

    float4 baseColor = getColor(bank, color);

    // Calculate luminance to detect dark vs light areas
    // Standard luminance formula weighted for human eye perception
    float luminance = dot(baseColor.rgb, float3(0.299, 0.587, 0.114));

    // Threshold for noise application (0.4 = apply noise to colors darker than 40% brightness)
    // Adjust this value to control which areas get noise:
    //   0.3 = only very dark areas (mostly shadows)
    //   0.4 = dark areas (clothing, shadows)
    //   0.5 = medium-dark areas (more aggressive)
    float noiseThreshold = 0.4;

    if (luminance < noiseThreshold)
    {
        // Dark area (clothing/shadows) - apply film grain noise
        // Screen-pixel noise stays stable as model depth changes. Multiplying
        // interpolated positions by depth produced moving moire/stripe patterns.
        uint2 noiseSampler = uint2(floor(IN.pos.xy)) % uint2(256, 256);
        float noise = float(fetch_s_noiseTexture( int2(noiseSampler), 0).r) / 255.f;

        // Scale noise strength based on how dark the area is
        // Darker areas get more pronounced noise
        float noiseStrength = (noiseThreshold - luminance) / noiseThreshold;
        outputColor = baseColor * (0.85f + (noise * 0.15f) * noiseStrength);
    }
    else
    {
        // Bright area (skin, face, hair) - no noise, just flat color
        outputColor = baseColor;
    }
    outputColor = applyModelAmbient(outputColor);

return outputColor; }

ENDHLSL
} }
}
