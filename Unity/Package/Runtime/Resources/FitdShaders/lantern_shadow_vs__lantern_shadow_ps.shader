// Generated from FitdLib/shaders/lantern_shadow_vs.sc and lantern_shadow_ps.sc. Regenerate with Unity/tools/convert_shaders.py.
Shader "Hidden/Fitd/lantern_shadow_vs/lantern_shadow_ps" {
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
 }
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







float4 u_lanternPos;       // xyz = light position (world space), w = distance attenuation
float4 u_lanternColor;     // xyz = light color, w = intensity
float4 u_lanternRadius;    // x = radius fraction of screen width (0.0-1.0)






Varyings vert(AppData IN) { Varyings OUT = (Varyings)0;

    // Standard screen-space position
    OUT.pos = float4(IN.position, 1.0);

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

    // Create a radial gradient centred on the actual viewport using bgfx built-in u_viewRect
    // u_viewRect: xy = viewport offset (pixels), zw = viewport size (pixels)
    float2 screenCenter = u_viewRect.xy + u_viewRect.zw * 0.5;
    float2 fragPos = IN.pos.xy;

    // Radius in pixels: fraction of viewport width stored in u_lanternRadius.x
    float radiusPx = u_lanternRadius.x * u_viewRect.z;
    float distFromCenter = length(fragPos - screenCenter) / radiusPx;
    float falloff = 1.0 - clamp(distFromCenter, 0.0, 1.0);
    falloff = falloff * falloff;  // Quadratic falloff for realistic decay

    float3 lightColor = u_lanternColor.xyz;
    float lightIntensity = u_lanternColor.w * falloff * 0.6;

    outputColor = float4(lightColor, lightIntensity);

return outputColor; }

ENDHLSL
} }
}
