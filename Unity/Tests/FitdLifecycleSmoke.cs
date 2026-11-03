using System;
using System.IO;
using System.Runtime.InteropServices;
using UnityEngine;
// Test-only bootstrap; copy into Assets, never the shipping runtime package.
public sealed class FitdLifecycleSmoke:MonoBehaviour {
 [DllImport("FitdUnity",CallingConvention=CallingConvention.Cdecl)] static extern void FitdUnity_RequestQuit();
 [DllImport("FitdUnity",CallingConvention=CallingConvention.Cdecl)] static extern int FitdUnity_GetState();
 [DllImport("FitdUnity",CallingConvention=CallingConvention.Cdecl)] static extern int FitdUnity_OptionsOpen();
 [DllImport("FitdUnity",CallingConvention=CallingConvention.Cdecl)] static extern void FitdUnity_ToggleOptions();
 [DllImport("FitdUnity",CallingConvention=CallingConvention.Cdecl)] static extern int FitdUnity_SetContentPaths([MarshalAs(UnmanagedType.LPUTF8Str)]string data,[MarshalAs(UnmanagedType.LPUTF8Str)]string saves);
 static string mode,result;double begun;bool requested;Fitd.Unity.FitdPlayer player;
 [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.BeforeSceneLoad)]static void Configure(){
  if(Application.isEditor)return;var args=Environment.GetCommandLineArgs();int flag=Array.IndexOf(args,"-fitdLifecycle");if(flag<0)return;
  mode=args[flag+1];int destination=Array.IndexOf(args,"-fitdResult");result=Path.GetFullPath(args[destination+1]);
  Directory.CreateDirectory(Path.GetDirectoryName(result));
  int source=Array.IndexOf(args,"-fitdSource");
  if(source>=0 && FitdUnity_SetContentPaths(Path.GetFullPath(args[source+1]),Path.Combine(Path.GetDirectoryName(result),"saves"))==0)throw new Exception("Could not configure lifecycle source");
 }
 [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.AfterSceneLoad)]static void Setup(){if(mode!=null)new GameObject("FITD lifecycle test").AddComponent<FitdLifecycleSmoke>();}
 void Awake(){begun=Time.realtimeSinceStartupAsDouble;}
 void Update(){
  if(Time.realtimeSinceStartupAsDouble-begun>120){File.WriteAllText(result,"FAIL: lifecycle timeout");Application.Quit(2);return;}
  if(player==null)player=FindObjectOfType<Fitd.Unity.FitdPlayer>();
  if(player==null || player.RenderedFrames<10 || requested)return;
  if(FitdUnity_OptionsOpen()!=0)FitdUnity_ToggleOptions();
  Application.quitting+=()=>{int state=FitdUnity_GetState();File.WriteAllText(result,state==3?"FITD_LIFECYCLE_PASS: "+mode+"; native stopped before Unity exit":"FAIL: native state="+state);};
  requested=true;
  if(mode=="engine")FitdUnity_RequestQuit();else Application.Quit(0);
 }
}
