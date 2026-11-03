// Generated from FitdLib/shaders/particle_vs.sc and particle_ps.sc. Regenerate with Unity/tools/convert_shaders.py.
Shader "Hidden/Fitd/particle_vs/particle_ps" {
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













Varyings vert(AppData IN) { Varyings OUT = (Varyings)0;

    // Screen-space orthographic projection - match background shader
    // Convert from 320x200 game space to normalized device coordinates (-1 to 1)
    float x = (IN.position.x / 160.0) - 1.0;  // 0-320 to -1..1
    float y = 1.0 - (IN.position.y / 100.0);  // 0-200 to 1..-1 (inverted)

    // IMPORTANT: Use z=0.0 to match other screen-space overlays
    // Screen-space rendering doesn't need depth; always render on top
    OUT.pos = float4(x, y, 0.0, 1.0);

    // Pass through alpha and isDirt flag to pixel shader
    OUT.color0 = IN.color0;  // alpha
    OUT.color1 = IN.color1;  // isDirt (0.0 = white dust, 1.0 = brown dirt)
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

    // Choose color based on particle type
    // IN.color1.x == 0.0 -> white dust (atmospheric)
    // IN.color1.x == 1.0 -> brown dirt (car particles)

    float3 dustColor = float3(0.9, 0.9, 0.85);   // Warm white for dust
    float3 dirtColor = float3(0.55, 0.42, 0.30); // Brown/tan for dirt

    float2 centered = IN.uv0 * 2.0 - 1.0;
    float radius2 = dot(centered, centered);
    float softShape = 1.0 - smoothstep(0.12, 1.0, radius2);
    // Dirt is denser in the core; airborne dust stays broad and translucent.
    float core = lerp(0.55 + softShape * 0.45, softShape, IN.color1.x);
    float alpha = IN.color0.x * softShape * core;
    if (alpha < 0.004)
        discard;

    float3 finalColor = lerp(dustColor, dirtColor, IN.color1.x);
    finalColor *= 0.82 + softShape * 0.18;
    outputColor = float4(finalColor, alpha);

return outputColor; }

ENDHLSL
} }
}
