using System;
using UnityEngine;
public static class FitdAudioSmoke {
 public static void Check(Fitd.Unity.FitdPlayer player){
  var audio=player.GetComponent<Fitd.Unity.FitdAudioOutput>();
  if(audio==null || audio.CallbackCount<10)throw new Exception("Unity DSP callback is not running");
  if(audio.GetComponent<AudioSource>().clip!=null)throw new Exception("Game audio must not use a read-ahead streaming clip");
  Debug.Log("FITD_AUDIO_PASS: callbacks="+audio.CallbackCount+" nonSilent="+audio.NonSilentCallbackCount+" bufferedMs="+audio.BufferedMilliseconds.ToString("F2")+" maxCallbackMs="+audio.MaxCallbackMilliseconds.ToString("F2"));
 }
}
