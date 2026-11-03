Shader "Hidden/FitdTests/DepthProbe" {
Properties { _Cull("Cull",Float)=0 _FlipY("Flip Y",Float)=0 _Depth("Depth",Float)=0.5 _Color("Color",Color)=(1,1,1,1) }
SubShader { Pass { Cull [_Cull] ZTest LEqual ZWrite On
HLSLPROGRAM
#pragma vertex vert
#pragma fragment frag
#include "UnityCG.cginc"
float _Depth; float _FlipY; float4 _Color;
float4 vert(float4 position:POSITION):SV_POSITION {
 float4 clip=float4(position.xy,_Depth,1);
 clip.y=lerp(clip.y,-clip.y,_FlipY);
 #if UNITY_REVERSED_Z
 clip.z=1-clip.z;
 #endif
 return clip;
}
float4 frag():SV_Target{return _Color;}
ENDHLSL
} } }
