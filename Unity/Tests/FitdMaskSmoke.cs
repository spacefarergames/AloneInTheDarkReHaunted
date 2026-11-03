using System;
using UnityEngine;
using UnityEngine.Rendering;
public static class FitdMaskSmoke {
 public static void Run(){
  var shader=Resources.Load<Shader>("FitdShaders/maskBackground_vs__maskHDBackground_ps");
  var material=new Material(shader);var mask=new Texture2D(4,4,TextureFormat.R8,false,true);mask.filterMode=FilterMode.Point;mask.wrapMode=TextureWrapMode.Clamp;
  var bytes=new byte[16];for(int y=0;y<4;y++)for(int x=0;x<4;x++)bytes[y*4+x]=(byte)(x<2?0:255);mask.LoadRawTextureData(bytes);mask.Apply();
  var background=new Texture2D(1,1,TextureFormat.RGBA32,false,true);background.SetPixel(0,0,Color.red);background.Apply();
  material.SetTexture("s_maskPaletteTexture",mask);material.SetTexture("s_backgroundTexture",background);material.SetVector("u_fadeLevel",Vector4.one);
  material.SetInt("_SrcBlend",(int)BlendMode.SrcAlpha);material.SetInt("_DstBlend",(int)BlendMode.OneMinusSrcAlpha);material.SetInt("_ZTest",(int)CompareFunction.Always);material.SetInt("_ZWrite",0);material.SetInt("_Cull",0);material.SetInt("_ColorMask",15);
  var mesh=new Mesh();mesh.vertices=new[]{new Vector3(0,0,0),new Vector3(320,0,0),new Vector3(320,200,0),new Vector3(0,200,0)};mesh.uv=new[]{new Vector2(0,0),new Vector2(1,0),new Vector2(1,1),new Vector2(0,1)};mesh.triangles=new[]{0,1,2,0,2,3};
  var target=new RenderTexture(16,16,24,RenderTextureFormat.ARGB32,RenderTextureReadWrite.Linear);target.Create();var previous=RenderTexture.active;var command=new CommandBuffer();command.SetRenderTarget(target);command.ClearRenderTarget(RTClearFlags.All,Color.green,1,0);command.DrawMesh(mesh,Matrix4x4.identity,material);Graphics.ExecuteCommandBuffer(command);
  RenderTexture.active=target;var image=new Texture2D(16,16,TextureFormat.RGBA32,false,true);image.ReadPixels(new Rect(0,0,16,16),0,0);image.Apply();Color hole=image.GetPixel(4,8),solid=image.GetPixel(12,8);
  if(hole.g<0.9f || hole.r>0.1f || solid.r<0.9f || solid.g>0.1f)throw new Exception("Mask coverage failed: transparent="+hole+" opaque="+solid);
  RenderTexture.active=previous;command.Dispose();foreach(var resource in new UnityEngine.Object[]{material,mask,background,mesh,target,image})UnityEngine.Object.DestroyImmediate(resource);
  Debug.Log("FITD_MASK_GPU_PASS: transparent mask preserves model; opaque mask occludes it");
 }
 public static void CheckNativeBindings(){
  int count=0;
  foreach(var material in Resources.FindObjectsOfTypeAll<Material>())if(material.shader.name=="Hidden/Fitd/maskBackground_vs/maskHDBackground_ps" || material.shader.name=="Hidden/Fitd/maskBackground_vs/maskBackground_ps"){
   var mask=material.GetTexture("s_maskPaletteTexture");
   if(mask==null || !mask.name.StartsWith("FITD texture"))throw new Exception("Native mask sampler is unbound: "+material.shader.name);
   count++;
  }
  if(count==0)throw new Exception("No native mask draws were exercised");
  Debug.Log("FITD_MASK_BINDING_PASS: "+count+" native mask materials bind the actual mask texture");
 }
}
