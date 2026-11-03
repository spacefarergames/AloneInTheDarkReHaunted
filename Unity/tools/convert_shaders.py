"""Generate Unity ShaderLab equivalents of the renderer's source shader pairs.

Run from any directory with Python 3. Generated shaders are committed source assets;
the player and the Unity bundle builder do not require Python or bgfx shader binaries.
"""
from pathlib import Path
import re

root = Path(__file__).resolve().parents[2]
source = root / 'FitdLib/shaders'
destination = root / 'Unity/Package/Runtime/Resources/FitdShaders'
destination.mkdir(parents=True, exist_ok=True)
pairs = set()
for cpp in (root / 'FitdLib').glob('*.cpp'):
    pairs.update(re.findall(r'loadBgfxProgram\s*\(\s*"(\w+)"\s*,\s*"(\w+)"', cpp.read_text(encoding="utf-8", errors="replace")))
pairs.update(re.findall(r'addShaderProgram\((\w+)\s+(\w+)\s', (root / 'FitdLib/CMakeLists.txt').read_text(encoding="utf-8")))
pairs.update([('vs_ocornut_imgui', 'fs_ocornut_imgui'), ('vs_imgui_image', 'fs_imgui_image')])

def expand(text, seen=None):
    seen = set() if seen is None else seen
    def include(match):
        name = match[1]
        if name == 'bgfx_shader.sh' or name in seen:
            return ''
        seen.add(name)
        return expand((source / name).read_text(encoding='utf-8'), seen)
    return re.sub(r'#include\s*[<"]([^>"]+)[>"]', include, text)

def functions(text):
    text = re.sub(r'^\$.*$', '', expand(text).replace('\ufeff',''), flags=re.M)
    match = re.search(r'void\s+main\s*\(\s*\)\s*\{', text)
    if not match:
        raise ValueError('Missing shader main')
    start = match.end(); depth = 1; end = start
    while depth:
        if text[end] == '{': depth += 1
        if text[end] == '}': depth -= 1
        end += 1
    return text[:match.start()] + text[end:], text[start:end-1]

varyings = {
    'v_texcoord0': ('float2', 'uv0', 'TEXCOORD0'),
    'v_sphereParams': ('float4', 'sphere', 'TEXCOORD1'),
    'v_screenSpacePosition': ('float3', 'screen', 'TEXCOORD2'),
    'v_color0': ('float4', 'color0', 'COLOR0'),
    'v_color1': ('float4', 'color1', 'COLOR1'),
}
attributes = {'a_position': 'IN.position', 'a_texcoord0': 'IN.uv0', 'a_texcoord1': 'IN.uv1', 'a_color0': 'IN.color0', 'a_color1': 'IN.color1'}

def translate(text):
    text = re.sub(r'\buniform\s+', '', text)
    for prefix, target in [('vec', 'float'), ('ivec', 'int'), ('uvec', 'uint')]:
        for n in range(2,5):
            text = re.sub(r'\b'+prefix+str(n)+r'\b', target+str(n), text)
    text = re.sub(r'\btexture2D\b', 'tex2D', text)
    text = re.sub(r'texelFetch\(\s*(\w+)\s*,', r'fetch_\1(', text)
    for old, new in [('mix','lerp'),('fract','frac'),('inversesqrt','rsqrt')]:
        text = re.sub(r'\b'+old+r'\b',new,text)
    return text

properties = '''
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
'''
render_state = '''
Cull [_Cull]
ZTest [_ZTest]
ZWrite [_ZWrite]
ColorMask [_ColorMask]
Blend [_SrcBlend] [_DstBlend], [_SrcBlendAlpha] [_DstBlendAlpha]
BlendOp [_BlendOp], [_BlendOpAlpha]
Stencil { Ref [_StencilRef] ReadMask [_StencilMask] Comp [_StencilComp]
Pass [_StencilPass] Fail [_StencilFail] ZFail [_StencilZFail] }
'''
common = '''
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
'''

metadata=[]
for vs, ps in sorted(pairs):
    if vs.startswith('vs_'):
        vp = ''; vb = 'gl_Position = mul(_FitdViewProj, float4(IN.position.xy, 0, 1)); v_texcoord0 = IN.uv0; v_color0 = IN.color0;'
        pp = 'SAMPLER2D(s_tex, 0);'
        pb = 'gl_FragColor = tex2D(s_tex, v_texcoord0) * v_color0;'
    else:
        vp, vb = functions((source / (vs+'.sc')).read_text(encoding='utf-8'))
        pp, pb = functions((source / (ps+'.sc')).read_text(encoding='utf-8'))
    for name, value in attributes.items():
        vb = re.sub(r'\b'+name+r'\b', value, vb)
    vb = vb.replace('gl_Position', 'OUT.pos')
    pb = pb.replace('gl_FragColor', 'outputColor').replace('gl_FragCoord', 'IN.pos')
    pb = re.sub(r'\breturn\s*;', 'return outputColor;', pb)
    for name, (_, field, _) in varyings.items():
        vb = re.sub(r'\b'+name+r'\b', 'OUT.'+field, vb)
        pb = re.sub(r'\b'+name+r'\b', 'IN.'+field, pb)
    prefix = translate(vp + '\n' + pp)
    sampler_code = []
    sampler_names = []
    def sampler(match):
        unsigned, name = match[1], match[2]
        sampler_names.append(name)
        result = f'sampler2D {name}; float4 {name}_TexelSize; float {name}_FitdFlipY; float {name}_FitdDepthReverse;\n'
        result += f'float4 sample_{name}(float2 uv) {{ uv.y=lerp(uv.y,1.0-uv.y,{name}_FitdFlipY); float4 value=tex2D({name},uv); value.r=lerp(value.r,1.0-value.r,{name}_FitdDepthReverse); return value; }}\n'
        result += f'float4 lod_{name}(float2 uv,float lod) {{ uv.y=lerp(uv.y,1.0-uv.y,{name}_FitdFlipY); float4 value=tex2Dlod({name},float4(uv,0,lod)); value.r=lerp(value.r,1.0-value.r,{name}_FitdDepthReverse); return value; }}\n'
        if unsigned:
            result = f'Texture2D<float4> {name};\nuint4 fetch_{name}(int2 p,int lod) {{ return (uint4)round({name}.Load(int3(p,lod))*255.0); }}\n'
        sampler_code.append(result)
        return ''
    prefix = re.sub(r'\b(U?)SAMPLER2D\(\s*(\w+)\s*,\s*\d+\s*\);', sampler, prefix)
    # A uniform can be declared by both stages; keep one declaration.
    declared = set()
    def uniform(match):
        name = match[2]
        if name in declared: return ''
        declared.add(name); return match[0]
    prefix = re.sub(r'\b(float[234](?:x[234])?)\s+(u_\w+)\s*;', uniform, prefix)
    prefix = re.sub(r'\btex2D\(\s*(s_\w+)\s*,',r'sample_\1(',prefix)
    prefix = re.sub(r'\btexture2DLod\(\s*(s_\w+)\s*,',r'lod_\1(',prefix)
    metadata.append((f'Hidden/Fitd/{vs}/{ps}', sorted(set(sampler_names+re.findall(r'\b(?:float[234](?:x[234])?)\s+(u_\w+)',prefix)))))
    program = common + '\n'.join(sampler_code) + prefix
    program += '\nVaryings vert(AppData IN) { Varyings OUT = (Varyings)0;\n' + translate(vb)
    # Preserve the old [0,1] depth convention across Unity's reversed-depth backends.
    program += '\n#if UNITY_UV_STARTS_AT_TOP\nOUT.pos.y = -OUT.pos.y;\n#endif\n#if defined(UNITY_REVERSED_Z)\nOUT.pos.z = OUT.pos.w - OUT.pos.z;\n#elif defined(SHADER_API_GLCORE) || defined(SHADER_API_GLES) || defined(SHADER_API_GLES3)\nOUT.pos.z = OUT.pos.z * 2.0 - OUT.pos.w;\n#endif\nreturn OUT; }\n'
    fragment = translate(pb)
    fragment = re.sub(r'\btex2D\(\s*(s_\w+)\s*,',r'sample_\1(',fragment)
    fragment = re.sub(r'\btexture2DLod\(\s*(s_\w+)\s*,',r'lod_\1(',fragment)
    program += '\nfloat4 frag(Varyings IN) : SV_Target { float4 outputColor = 0;\n' + fragment + '\nreturn outputColor; }\n'
    texture_properties = ''.join(f'\n{name}("{name}", 2D) = "white" {{}}' for name in sampler_names)
    shader = f'// Generated from FitdLib/shaders/{vs}.sc and {ps}.sc. Regenerate with Unity/tools/convert_shaders.py.\nShader "Hidden/Fitd/{vs}/{ps}" {{\nProperties {{ {properties}{texture_properties} }}\nSubShader {{ Tags {{ "RenderType"="Opaque" }}\nPass {{ {render_state}\nHLSLPROGRAM\n#pragma target 3.5\n#pragma vertex vert\n#pragma fragment frag\n#include "UnityCG.cginc"\n{program}\nENDHLSL\n}} }}\n}}\n'
    (destination / f'{vs}__{ps}.shader').write_text(shader, encoding="utf-8")

entries='\n'.join('            {"'+name+'",new HashSet<string>{'+','.join('"'+v+'"' for v in names)+'}},' for name,names in metadata)
(destination.parents[1]/'FitdShaderUniforms.cs').write_text('''// Generated by Unity/tools/convert_shaders.py.
using System.Collections.Generic;
namespace Fitd.Unity {
internal static class FitdShaderUniforms {
    private static readonly Dictionary<string,HashSet<string>> names=new Dictionary<string,HashSet<string>> {
'''+entries+'''
    };
    internal static HashSet<string> Get(string shader){HashSet<string> result;return names.TryGetValue(shader,out result)?result:null;}
}
}
''',encoding='utf-8')

native_entries='\n'.join('        {"'+name.removeprefix('Hidden/Fitd/')+'",{'+','.join('"'+v+'"' for v in names)+'}},' for name,names in metadata)
(root/'Unity/Native/generatedShaderUniforms.h').write_text('''// Generated by Unity/tools/convert_shaders.py.
#pragma once
#include <string>
#include <unordered_map>
#include <unordered_set>
inline const std::unordered_set<std::string>* fitdShaderUniforms(const std::string& vertex,const std::string& fragment) {
    static const std::unordered_map<std::string,std::unordered_set<std::string>> names={
'''+native_entries+'''
    };
    auto found=names.find(vertex+"/"+fragment);return found==names.end()?nullptr:&found->second;
}
''',encoding='utf-8')

print(f'Generated {len(pairs)} Unity shader pairs in {destination}')
