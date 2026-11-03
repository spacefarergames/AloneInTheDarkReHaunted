// Copy into the prepared project's Assets/Editor for batch-mode verification.
using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine;
using System.IO;
using System.Runtime.InteropServices;

[InitializeOnLoad]
public static class FitdUnitySmoke
{
    [DllImport("FitdUnity",CallingConvention=CallingConvention.Cdecl)] private static extern int FitdUnity_OptionsOpen();
    [DllImport("FitdUnity",CallingConvention=CallingConvention.Cdecl)] private static extern void FitdUnity_ToggleOptions();
    [DllImport("FitdUnity",CallingConvention=CallingConvention.Cdecl)] private static extern void FitdUnity_GetSceneInfo(out int game,out int floor,out int camera,out int active);
    [DllImport("FitdUnity",CallingConvention=CallingConvention.Cdecl)] private static extern int FitdUnity_SetContentPaths([MarshalAs(UnmanagedType.LPUTF8Str)]string root,[MarshalAs(UnmanagedType.LPUTF8Str)]string saves);
    [DllImport("FitdUnity",CallingConvention=CallingConvention.Cdecl)] private static extern void FitdUnity_GetGpuTimings([Out]double[] values,int count);
    [DllImport("FitdUnity",CallingConvention=CallingConvention.Cdecl)] private static extern int FitdUnity_GetFontStatus();
    private static bool Gameplay => System.Array.Exists(System.Environment.GetCommandLineArgs(),argument=>argument=="-fitdGameplay");
    private static int polls;
    private const string Active = "FITD.UnitySmoke.Active";
    private const string Started = "FITD.UnitySmoke.Started";
    private const string Phase = "FITD.UnitySmoke.Phase";
    static FitdUnitySmoke() { EditorApplication.update += Poll; }
    public static void Run()
    {
        FitdShadersSmoke.Run();
        FitdFacingSmoke.Run();
        FitdMaskSmoke.Run();
        var gameView=System.Type.GetType("UnityEditor.GameView,UnityEditor");
        if(gameView!=null)EditorWindow.GetWindow(gameView);
        EditorSceneManager.NewScene(NewSceneSetup.EmptyScene, NewSceneMode.Single);
        new GameObject("Main Camera", typeof(Camera), typeof(AudioListener)).tag = "MainCamera";
        var player = new GameObject("FITD Smoke").AddComponent<Fitd.Unity.FitdPlayer>();
        player.renderWidth = 960; player.renderHeight = 600;
        player.useAssetBundles = !System.Array.Exists(System.Environment.GetCommandLineArgs(), argument=>argument=="-fitdLooseData");
        var arguments=System.Environment.GetCommandLineArgs();int sourceIndex=System.Array.IndexOf(arguments,"-fitdSource");
        if(sourceIndex>=0){player.useAssetBundles=false;if(FitdUnity_SetContentPaths(Path.GetFullPath(arguments[sourceIndex+1]),Path.Combine(Directory.GetCurrentDirectory(),"fitd-test-saves"))==0)throw new System.Exception("Could not set smoke-test source paths");}
        EditorSceneManager.SaveScene(player.gameObject.scene, "Assets/FitdPlayer.unity");
        // MonoBehaviours must not start the host while editing a scene.
        SessionState.SetBool(Active, true);SessionState.SetBool("FITD.WorldSeen",false);
        SessionState.SetInt(Phase, 0);
        SessionState.SetFloat(Started, (float)EditorApplication.timeSinceStartup);
        EditorApplication.isPlaying = true;
    }
    private static void Poll()
    {
        if (!SessionState.GetBool(Active, false)) return;polls++;
        if (EditorApplication.timeSinceStartup - SessionState.GetFloat(Started, 0) > 600) Finish(2, "Timed out waiting for Unity frame");
        if (!EditorApplication.isPlaying)
        {
            if (SessionState.GetInt(Phase, 0) == 1 && !EditorApplication.isPlayingOrWillChangePlaymode)
            {
                SessionState.SetInt(Phase, 2);
                EditorApplication.isPlaying = true;
            }
            return;
        }
        var player = Object.FindObjectOfType<Fitd.Unity.FitdPlayer>();
        if (player == null) return;
        if(SessionState.GetInt(Phase,0)==3){if(FitdInputSmoke.Error!=null)Finish(6,FitdInputSmoke.Error);else if(FitdInputSmoke.Done)Finish(0,"PASS: bundled gameplay, facing, font, input and Play-mode resume");return;}
        if (player.Error != null) { Finish(3, player.Error); return; }
        if (player.Frame == null || player.RenderedFrames == 0) return;
        if(SessionState.GetInt(Phase,0)==0 && FitdUnity_OptionsOpen()!=0)FitdUnity_ToggleOptions();
        if(Gameplay && SessionState.GetInt(Phase,0)==2)
        {
            player.automaticInput=false;
            if(FitdUnity_OptionsOpen()!=0)FitdUnity_ToggleOptions();
            int game,floor,camera,active;FitdUnity_GetSceneInfo(out game,out floor,out camera,out active);
            if(active==0 || floor!=0)
            {
                double tick=EditorApplication.timeSinceStartup;
                player.SubmitInput(0,tick%2<0.25?0x1c:0,false,new Vector2(-1,-1));
                return;
            }
            player.SubmitInput(0,0,false,new Vector2(-1,-1));
            if(!SessionState.GetBool("FITD.WorldSeen",false)){FitdPerfCapture.Begin();SessionState.SetBool("FITD.WorldSeen",true);SessionState.SetFloat("FITD.WorldStarted",(float)EditorApplication.timeSinceStartup);SessionState.SetInt("FITD.WorldFrames",(int)player.RenderedFrames);SessionState.SetInt("FITD.Polls",polls);var timing=new double[4];FitdUnity_GetGpuTimings(timing,4);for(int n=0;n<4;n++)SessionState.SetFloat("FITD.NativeTiming"+n,(float)timing[n]);SessionState.SetFloat("FITD.WorldCpu",(float)player.RendererTotalCpuMilliseconds);}
            if(EditorApplication.timeSinceStartup-SessionState.GetFloat("FITD.WorldStarted",0)<15)return;
            FitdPerfCapture.End();
            FitdMaskSmoke.CheckNativeBindings();
            FitdPresentationSmoke.Check(player);
            FitdAudioSmoke.Check(player);
            if(FitdUnity_GetFontStatus()!=2){Finish(5,"Configured game TTF was not loaded");return;}
            Debug.Log("FITD_FONT_PASS: configured TTF loaded independently of Unity working directory");
            var latest=new double[4];FitdUnity_GetGpuTimings(latest,4);for(int n=0;n<4;n++)latest[n]-=SessionState.GetFloat("FITD.NativeTiming"+n,0);Debug.Log("FITD_NATIVE_TIMING: workMs="+(latest[1]/System.Math.Max(1,latest[0])).ToString("F2")+" waitMs="+(latest[2]/System.Math.Max(1,latest[0])).ToString("F2")+" encodeMs="+(latest[3]/System.Math.Max(1,latest[0])).ToString("F2"));
            Debug.Log("FITD_EDITOR_TICK: hz="+((polls-SessionState.GetInt("FITD.Polls",0))/(EditorApplication.timeSinceStartup-SessionState.GetFloat("FITD.WorldStarted",0))).ToString("F2")+" focused="+Application.isFocused+" target="+Application.targetFrameRate+" background="+Application.runInBackground);
            Debug.Log("FITD_PERFORMANCE: fps="+((player.RenderedFrames-SessionState.GetInt("FITD.WorldFrames",0))/(EditorApplication.timeSinceStartup-SessionState.GetFloat("FITD.WorldStarted",0))).ToString("F1")+" meanRenderCpuMs="+((player.RendererTotalCpuMilliseconds-SessionState.GetFloat("FITD.WorldCpu",0))/System.Math.Max(1,player.RenderedFrames-SessionState.GetInt("FITD.WorldFrames",0))).ToString("F2")+" lastRenderCpuMs="+player.RendererCpuMilliseconds.ToString("F2")+" draws="+player.RendererDrawCalls);
            Debug.Log("FITD_GAMEPLAY_PASS: game="+game+" floor="+floor+" camera="+camera);
        }
        Texture2D image = player.Frame as Texture2D;
        bool temporary = image == null;
        if (temporary)
        {
            var previous = RenderTexture.active;
            RenderTexture.active = player.Frame as RenderTexture;
            image = new Texture2D(player.Frame.width,player.Frame.height,TextureFormat.RGBA32,false,true);
            image.ReadPixels(new Rect(0,0,image.width,image.height),0,0);image.Apply();
            RenderTexture.active = previous;
        }
        try {
        if (image.width != 960 || image.height != 600) { Finish(4, "Unexpected native texture size"); return; }
        var pixels = image.GetPixels32();
        foreach (var pixel in pixels)
            if (pixel.r > 20 || pixel.g > 20 || pixel.b > 20)
            {
                if (SessionState.GetInt(Phase, 0) == 0)
                {
                    SessionState.SetInt(Phase, 1);
                    EditorApplication.isPlaying = false;
                    return;
                }
                var diagnostics=new System.Collections.Generic.HashSet<string>();
                foreach(var material in Resources.FindObjectsOfTypeAll<Material>())if(material.shader.name.StartsWith("Hidden/Fitd/")){
                    var palette=material.HasProperty("s_paletteTexture")?material.GetTexture("s_paletteTexture"):null;
                    var model=material.HasProperty("s_modelTexture")?material.GetTexture("s_modelTexture") as Texture2D:null;
                    if(model!=null)File.WriteAllBytes("gpu-model-"+model.name.Replace(" ","_")+".png",model.EncodeToPNG());
                    diagnostics.Add(material.shader.name+" ambient="+material.GetVector("u_modelAmbient")+" palette="+(palette!=null?palette.name+" texel="+material.GetVector("s_paletteTexture_TexelSize"):"none")+" z="+material.GetInt("_ZTest")+" write="+material.GetInt("_ZWrite"));
                }
                File.WriteAllLines("gpu-materials.txt",diagnostics);
                foreach(var target in Resources.FindObjectsOfTypeAll<RenderTexture>())if(target.name.StartsWith("FITD texture")){
                    var previousTarget=RenderTexture.active;
                    var copy=RenderTexture.GetTemporary(target.width,target.height,0,RenderTextureFormat.ARGB32,RenderTextureReadWrite.Linear);
                    Graphics.Blit(target,copy);RenderTexture.active=copy;
                    var captured=new Texture2D(target.width,target.height,TextureFormat.RGBA32,false,true);
                    captured.ReadPixels(new Rect(0,0,target.width,target.height),0,0);captured.Apply();
                    File.WriteAllBytes("gpu-target-"+target.name.Replace(" ","_")+".png",captured.EncodeToPNG());
                    RenderTexture.active=previousTarget;Object.DestroyImmediate(captured);RenderTexture.ReleaseTemporary(copy);
                }
                var meshInfo=new System.Collections.Generic.List<string>();
                foreach(var mesh in Resources.FindObjectsOfTypeAll<Mesh>())if(mesh.name.StartsWith("FITD draw")){
                    var uv=new System.Collections.Generic.List<Vector4>();mesh.GetUVs(0,uv);
                    if(uv.Count>6)meshInfo.Add(mesh.name+" vertices="+mesh.vertexCount+" uv="+uv[0]+","+uv[3]+","+uv[6]);
                }
                File.WriteAllLines("gpu-meshes.txt",meshInfo);
                foreach(var texture in Resources.FindObjectsOfTypeAll<Texture2D>())if(texture.name.StartsWith("FITD texture") && texture.width==3 && texture.height==256)File.WriteAllBytes("gpu-palette.png",texture.EncodeToPNG());
                File.WriteAllBytes(Path.Combine(Directory.GetCurrentDirectory(), "unity-smoke.png"), image.EncodeToPNG());
                if(System.Array.Exists(System.Environment.GetCommandLineArgs(),argument=>argument=="-fitdInput")){SessionState.SetInt(Phase,3);FitdInputSmoke.Start(player);return;}
                Finish(0, "PASS: Unity C++ gameplay, rendered frames, and Play-mode/domain reload resume");
                return;
            }
        } finally { if(temporary)Object.DestroyImmediate(image); }
    }
    private static void Finish(int code, string message)
    {
        SessionState.SetBool(Active, false);
        Debug.Log(message);
        Fitd.Unity.FitdPlayer.FitdUnity_Stop();
        EditorApplication.Exit(code);
    }
}
