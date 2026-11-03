// Generated from FitdLib/shaders/background_vs.sc and hdBackground_ps.sc. Regenerate with Unity/tools/convert_shaders.py.
Shader "Hidden/Fitd/background_vs/hdBackground_ps" {
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

s_backgroundTexture("s_backgroundTexture", 2D) = "white" {} }
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
sampler2D s_backgroundTexture; float4 s_backgroundTexture_TexelSize; float s_backgroundTexture_FitdFlipY; float s_backgroundTexture_FitdDepthReverse;
float4 sample_s_backgroundTexture(float2 uv) { uv.y=lerp(uv.y,1.0-uv.y,s_backgroundTexture_FitdFlipY); float4 value=tex2D(s_backgroundTexture,uv); value.r=lerp(value.r,1.0-value.r,s_backgroundTexture_FitdDepthReverse); return value; }
float4 lod_s_backgroundTexture(float2 uv,float lod) { uv.y=lerp(uv.y,1.0-uv.y,s_backgroundTexture_FitdFlipY); float4 value=tex2Dlod(s_backgroundTexture,float4(uv,0,lod)); value.r=lerp(value.r,1.0-value.r,s_backgroundTexture_FitdDepthReverse); return value; }












float4 u_fadeLevel;       // x = fade level (0.0 = black, 1.0 = full brightness)

// Lantern lighting uniforms
// u_lanternLight:      xy = screen-space UV position (0..1), z = effective intensity (0..1), w = 1/radius^2
// u_lanternLightColor: xyz = light color (warm orange), w = backgroundLightInfluence (0..1)
// u_lanternAmbient:    x = ambient darkness multiplier for unlit areas (0..1, 1 = no effect)
float4 u_lanternLight;
float4 u_lanternLightColor;
float4 u_lanternAmbient;



Varyings vert(AppData IN) { Varyings OUT = (Varyings)0;

    OUT.pos = float4(IN.position.x/160.0-1.0, 1.0-IN.position.y/100.0, 0.0, 1.0);
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

    // HD backgrounds use RGBA8 format with direct color sampling (no palette lookup)
    float4 color = sample_s_backgroundTexture( IN.uv0);
    float3 rgb = color.rgb;

    // --- Luminance-guided real-time lantern lighting ---
    // Compute perceptual luminance of the background pixel.
    // This acts as our surface mask: bright areas (lit walls, floors) reflect lantern light
    // more readily; dark areas (shadow corners, recesses) absorb it and stay dark.
    float luminance = dot(rgb, float3(0.299, 0.587, 0.114));

    float lanternIntensity  = u_lanternLight.z;       // 0..1
    float influence         = u_lanternLightColor.w;  // backgroundLightInfluence 0..1

    if (lanternIntensity > 0.001 && influence > 0.001)
    {
        // Distance from this pixel to the lantern screen position
        float2  lanternUV   = u_lanternLight.xy;
        float invRadiusSq = u_lanternLight.w;         // 1 / radius^2 (in UV space)
        float2  delta       = IN.uv0 - lanternUV;
        float distSq      = dot(delta, delta);

        // Smooth quadratic attenuation — falls off naturally like a real point light
        float attenuation = clamp(1.0 - distSq * invRadiusSq, 0.0, 1.0);
        attenuation       = attenuation * attenuation; // squared for softer roll-off

        // Luminance-based surface factor:
        //   - Already-bright pixels (luminance ~1) pick up full lantern color
        //   - Dark pixels (luminance ~0) receive a smaller fraction — they're in shadow,
        //     the lantern light can't fully reach them, but the warm glow still bleeds in slightly
        float surfaceFactor = lerp(0.15, 1.0, luminance);

        // Combined light contribution
        float lightAmount = attenuation * lanternIntensity * influence * surfaceFactor;

        float3 lanternColor = u_lanternLightColor.xyz;

        // Additive warm light on bright surfaces; also slightly warm-shifts dark areas
        rgb = rgb + lanternColor * lightAmount;

        // Shadow deepening in unlit areas far from lantern:
        // Dark pixels that are far from the light source get pushed darker — they are true
        // shadow volumes now that the lantern is the dominant light in the room.
        float shadowFactor = u_lanternAmbient.x; // 1.0 = no darkening, < 1.0 = deepen shadows
        float farFromLight  = 1.0 - attenuation;
        float darkMask      = (1.0 - luminance);  // Only affects already-dark areas
        float darken        = 1.0 - (farFromLight * darkMask * (1.0 - shadowFactor) * influence);
        rgb = rgb * darken;
    }

    // Clamp to valid range
    rgb = clamp(rgb, vec3_splat(0.0), vec3_splat(1.0));

    // Apply fade level for fade in/out effects
    outputColor = float4(rgb * u_fadeLevel.x, color.a);

return outputColor; }

ENDHLSL
} }
}
