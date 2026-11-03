using System;
using System.Reflection;
using UnityEditor;
using UnityEngine;
using UnityEngine.Rendering;
public static class FitdFacingSmoke {
 public static void Run(){
  var shader=AssetDatabase.LoadAssetAtPath<Shader>("Assets/Editor/FitdDepthProbe.shader");
  var material=new Material(shader);material.SetColor("_Color",Color.green);material.SetFloat("_Depth",0.5f);material.SetFloat("_FlipY",SystemInfo.graphicsUVStartsAtTop?1:0);
  var mesh=new Mesh();mesh.vertices=new[]{new Vector3(-1,-1,0),new Vector3(1,-1,0),new Vector3(1,1,0),new Vector3(-1,1,0)};mesh.triangles=new[]{0,2,1,0,3,2};
  var target=new RenderTexture(8,8,24,RenderTextureFormat.ARGB32,RenderTextureReadWrite.Linear);target.Create();
  var command=new CommandBuffer();var previous=RenderTexture.active;
  var configure=typeof(Fitd.Unity.FitdGpuRenderer).GetMethod("ConfigureState",BindingFlags.NonPublic|BindingFlags.Static);
  foreach(bool reverse in new[]{false,true}){
   configure.Invoke(null,new object[]{material,0x200000008fUL|(reverse?0x8000000000UL:0UL),0u,0u});
   command.Clear();command.SetRenderTarget(target);command.ClearRenderTarget(RTClearFlags.All,Color.black,1,0);command.DrawMesh(mesh,Matrix4x4.identity,material);Graphics.ExecuteCommandBuffer(command);
   RenderTexture.active=target;var image=new Texture2D(8,8,TextureFormat.RGBA32,false,true);image.ReadPixels(new Rect(0,0,8,8),0,0);image.Apply();bool visible=image.GetPixel(4,4).g>0.9f;UnityEngine.Object.DestroyImmediate(image);
   if(visible==reverse)throw new Exception("Native clockwise face/front-CCW conversion failed, reverse="+reverse+" visible="+visible);
  }
  RenderTexture.active=previous;command.Dispose();UnityEngine.Object.DestroyImmediate(mesh);UnityEngine.Object.DestroyImmediate(material);UnityEngine.Object.DestroyImmediate(target);
  Debug.Log("FITD_FACING_PASS: native clockwise front faces remain visible; front-CCW flag reverses culling");
 }
}
