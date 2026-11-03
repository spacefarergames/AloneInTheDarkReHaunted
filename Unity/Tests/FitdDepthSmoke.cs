using UnityEditor;
using UnityEngine;
using UnityEngine.Rendering;
public static class FitdDepthSmoke
{
    public static void Run()
    {
        var color=new RenderTexture(4,4,0,RenderTextureFormat.ARGB32,RenderTextureReadWrite.Linear);
        var depth=new RenderTexture(4,4,24,RenderTextureFormat.Depth,RenderTextureReadWrite.Linear);
        color.Create();depth.Create();
        var command=new CommandBuffer();
        foreach(float value in new[]{0f,1f})
        {
            command.Clear();command.SetRenderTarget(color,depth);command.ClearRenderTarget(RTClearFlags.All,Color.black,value,0);
            Graphics.ExecuteCommandBuffer(command);
            Graphics.Blit(depth,color);RenderTexture.active=color;
            var image=new Texture2D(4,4,TextureFormat.RGBA32,false,true);image.ReadPixels(new Rect(0,0,4,4),0,0);image.Apply();
            Debug.Log("FITD_DEPTH_CLEAR: input="+value+" sampled="+image.GetPixel(2,2).r+" reversed="+SystemInfo.usesReversedZBuffer);
            float expected=SystemInfo.usesReversedZBuffer?1-value:value;
            if(Mathf.Abs(image.GetPixel(2,2).r-expected)>1f/255)throw new System.Exception("Unexpected depth-clear convention");
            Object.DestroyImmediate(image);
        }
        var shader=AssetDatabase.LoadAssetAtPath<Shader>("Assets/Editor/FitdDepthProbe.shader");
        var front=new Material(shader);front.SetFloat("_Depth",0.25f);front.SetColor("_Color",Color.green);
        var back=new Material(shader);back.SetFloat("_Depth",0.75f);back.SetColor("_Color",Color.red);
        var mesh=new Mesh();mesh.vertices=new[]{new Vector3(-1,-1,0),new Vector3(1,-1,0),new Vector3(1,1,0),new Vector3(-1,1,0)};mesh.triangles=new[]{0,1,2,0,2,3};
        command.Clear();command.SetRenderTarget(color,depth);command.ClearRenderTarget(RTClearFlags.All,Color.black,1,0);
        command.DrawMesh(mesh,Matrix4x4.identity,front);command.DrawMesh(mesh,Matrix4x4.identity,back);Graphics.ExecuteCommandBuffer(command);
        RenderTexture.active=color;var result=new Texture2D(4,4,TextureFormat.RGBA32,false,true);result.ReadPixels(new Rect(0,0,4,4),0,0);result.Apply();
        Color pixel=result.GetPixel(2,2);
        if(pixel.g<0.9f || pixel.r>0.1f)throw new System.Exception("Logical depth comparison failed: "+pixel);
        Debug.Log("FITD_DEPTH_PASS: logical clear/comparison with explicit shader depth conversion");
        Object.DestroyImmediate(result);Object.DestroyImmediate(mesh);Object.DestroyImmediate(front);Object.DestroyImmediate(back);
        RenderTexture.active=null;command.Dispose();Object.DestroyImmediate(color);Object.DestroyImmediate(depth);
    }
}
