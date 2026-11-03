using System.Collections.Generic;
using Unity.Profiling;
using Unity.Profiling.LowLevel.Unsafe;
using UnityEngine;
public static class FitdPerfCapture {
 static readonly List<ProfilerRecorder> recorders=new List<ProfilerRecorder>();
 static readonly List<string> names=new List<string>();
 public static void Begin(){
  var handles=new List<ProfilerRecorderHandle>();ProfilerRecorderHandle.GetAvailable(handles);
  foreach(var handle in handles){var info=ProfilerRecorderHandle.GetDescription(handle);
   if(info.UnitType.ToString()!="TimeNanoseconds")continue;
   if(info.Name=="Main Thread" || info.Name=="Render Thread" || info.Name=="PlayerLoop" || info.Name=="EditorLoop" || info.Name.Contains("Wait") || info.Name.Contains("Update") || info.Name.StartsWith("FITD.") || info.Name.StartsWith("GC.") || info.Name=="Camera.Render" || info.Name=="GUI.Repaint"){
    var recorder=ProfilerRecorder.StartNew(info.Category,info.Name,128);if(recorder.Valid){recorders.Add(recorder);names.Add(info.Name);}
   }
  }
 }
 public static void End(){
  for(int i=0;i<recorders.Count;i++){var samples=new List<ProfilerRecorderSample>();recorders[i].CopyTo(samples);double total=0,max=0;foreach(var sample in samples){total+=sample.Value;max=System.Math.Max(max,sample.Value);}if(total/System.Math.Max(1,samples.Count)>1e6 || names[i].StartsWith("FITD."))Debug.Log("FITD_PROFILE: "+names[i]+" meanMs="+(total/System.Math.Max(1,samples.Count)/1e6).ToString("F2")+" maxMs="+(max/1e6).ToString("F2")+" count="+samples.Count);recorders[i].Dispose();}
  recorders.Clear();names.Clear();
 }
}
