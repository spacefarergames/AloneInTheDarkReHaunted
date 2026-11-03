using System;
using System.IO;
using System.Runtime.InteropServices;
using UnityEngine;

// Test-only player bootstrap. Copy to Assets, never ship with the runtime package.
public sealed class FitdPlayerBenchmark : MonoBehaviour
{
    [DllImport("FitdUnity",CallingConvention=CallingConvention.Cdecl)] static extern int FitdUnity_SetContentPaths([MarshalAs(UnmanagedType.LPUTF8Str)]string content,[MarshalAs(UnmanagedType.LPUTF8Str)]string saves);
    [DllImport("FitdUnity",CallingConvention=CallingConvention.Cdecl)] static extern int FitdUnity_OptionsOpen();
    [DllImport("FitdUnity",CallingConvention=CallingConvention.Cdecl)] static extern void FitdUnity_ToggleOptions();
    [DllImport("FitdUnity",CallingConvention=CallingConvention.Cdecl)] static extern void FitdUnity_GetSceneInfo(out int game,out int floor,out int camera,out int active);
    [DllImport("FitdUnity",CallingConvention=CallingConvention.Cdecl)] static extern void FitdUnity_GetGpuTimings([Out]double[] values,int count);
    static bool enabledTest;
    static string output;
    Fitd.Unity.FitdPlayer player;
    double begun,warmup=-1,sample=-1,cpu;
    uint frames;
    double[] native=new double[4];
    [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.BeforeSceneLoad)]
    static void Configure()
    {
        if(Application.isEditor)return;
        var args=Environment.GetCommandLineArgs();int flag=Array.IndexOf(args,"-fitdBenchmark");
        if(flag<0)return;
        enabledTest=true;output=Path.GetFullPath(args[flag+1]);Directory.CreateDirectory(output);
        int source=Array.IndexOf(args,"-fitdSource");
        if(source>=0 && FitdUnity_SetContentPaths(Path.GetFullPath(args[source+1]),Path.Combine(output,"saves"))==0)throw new Exception("Could not configure test source");
        Application.runInBackground=true;QualitySettings.vSyncCount=0;Application.targetFrameRate=60;
    }
    [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.AfterSceneLoad)]
    static void StartRunner(){if(enabledTest)new GameObject("FITD benchmark").AddComponent<FitdPlayerBenchmark>();}
    void Awake(){begun=Time.realtimeSinceStartupAsDouble;}
    void Update()
    {
        double now=Time.realtimeSinceStartupAsDouble;
        if(now-begun>300){Finish(2,"Benchmark timed out");return;}
        if(player==null)player=FindObjectOfType<Fitd.Unity.FitdPlayer>();
        if(player==null)return;
        player.automaticInput=false;
        if(player.Error!=null){Finish(3,player.Error);return;}
        if(player.Frame==null || player.RenderedFrames==0)return;
        if(FitdUnity_OptionsOpen()!=0)FitdUnity_ToggleOptions();
        int game,floor,camera,active;FitdUnity_GetSceneInfo(out game,out floor,out camera,out active);
        if(active==0 || floor!=0){player.SubmitInput(0,now%2<0.25?0x1c:0,false,new Vector2(-1,-1));return;}
        player.SubmitInput(0,0,false,new Vector2(-1,-1));
        if(warmup<0)warmup=now;
        if(now-warmup<5)return;
        if(sample<0){sample=now;frames=player.RenderedFrames;cpu=player.RendererTotalCpuMilliseconds;FitdUnity_GetGpuTimings(native,4);}
        if(now-sample<15)return;
        var audio=player.GetComponent<Fitd.Unity.FitdAudioOutput>();
        if(audio==null || audio.CallbackCount<10){Finish(4,"Unity DSP audio callback did not run");return;}
        Debug.Log("FITD_AUDIO_PLAYER: callbacks="+audio.CallbackCount+" nonSilent="+audio.NonSilentCallbackCount+" bufferedMs="+audio.BufferedMilliseconds.ToString("F2")+" maxCallbackMs="+audio.MaxCallbackMilliseconds.ToString("F2"));
        var latest=new double[4];FitdUnity_GetGpuTimings(latest,4);for(int i=0;i<4;i++)latest[i]-=native[i];
        double count=Math.Max(1,player.RenderedFrames-frames);
        var previous=RenderTexture.active;RenderTexture.active=player.Frame as RenderTexture;
        var image=new Texture2D(player.Frame.width,player.Frame.height,TextureFormat.RGBA32,false,true);
        image.ReadPixels(new Rect(0,0,image.width,image.height),0,0);image.Apply();RenderTexture.active=previous;
        File.WriteAllBytes(Path.Combine(output,"frame.png"),image.EncodeToPNG());Destroy(image);
        Finish(0,"FITD_PLAYER_PERFORMANCE: fps="+(count/(now-sample)).ToString("F2")+" renderCpuMs="+((player.RendererTotalCpuMilliseconds-cpu)/count).ToString("F2")+" nativeWorkMs="+(latest[1]/Math.Max(1,latest[0])).ToString("F2")+" unityWaitMs="+(latest[2]/Math.Max(1,latest[0])).ToString("F2")+" floor="+floor+" camera="+camera);
    }
    void Finish(int code,string message){Debug.Log(message);File.WriteAllText(Path.Combine(output,"result.txt"),message);Application.Quit(code);}
}
