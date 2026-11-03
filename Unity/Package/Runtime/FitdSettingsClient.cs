using System;

using System.Collections.Generic;

using System.Runtime.InteropServices;

using System.Text;

using UnityEngine;

namespace Fitd.Unity {

 [Serializable] public sealed class FitdSetting { public int id;public string key,title,group,kind,description,section;public float min,max;public int parent=-1; }

 [Serializable] public sealed class FitdSettingsSchema {public FitdSetting[] settings;}

 public sealed class FitdSettingsClient {

  [DllImport(FitdPlayer.Library,CallingConvention=CallingConvention.Cdecl)] static extern int FitdUnity_SettingsCount();

  [DllImport(FitdPlayer.Library,CallingConvention=CallingConvention.Cdecl)] static extern int FitdUnity_GetSettings([Out]float[] values,int count);

  [DllImport(FitdPlayer.Library,CallingConvention=CallingConvention.Cdecl)] static extern void FitdUnity_SetSetting(int id,float value);

  [DllImport(FitdPlayer.Library,CallingConvention=CallingConvention.Cdecl)] static extern void FitdUnity_GetSettingText(int id,StringBuilder text,int capacity);

  [DllImport(FitdPlayer.Library,CallingConvention=CallingConvention.Cdecl)] static extern void FitdUnity_SetSettingText(int id,[MarshalAs(UnmanagedType.LPUTF8Str)]string text);

  [DllImport(FitdPlayer.Library,CallingConvention=CallingConvention.Cdecl)] static extern int FitdUnity_SettingsAction(int action);

  [DllImport(FitdPlayer.Library,CallingConvention=CallingConvention.Cdecl)] static extern int FitdUnity_SettingsResult(out int status);

  [DllImport(FitdPlayer.Library,CallingConvention=CallingConvention.Cdecl)] internal static extern int FitdUnity_OptionsOpen();

  [DllImport(FitdPlayer.Library,CallingConvention=CallingConvention.Cdecl)] internal static extern void FitdUnity_SetOptionsOpen(int open);

  [DllImport(FitdPlayer.Library,CallingConvention=CallingConvention.Cdecl)] internal static extern int FitdUnity_OptionsStartup();

  public readonly FitdSetting[] Settings;

  public readonly float[] Values;

  public readonly string[] Text=new string[2];

  readonly float[] raw;

  readonly Dictionary<int,float> pending=new Dictionary<int,float>();

  readonly FitdPlayer player;

  int revision,request,action;double nextPoll;

  public bool Ready=>revision>0;

  public bool Busy=>request!=0;

  public float Volume {get;private set;}

  public bool ReduceMotion {get;private set;}

  public float UiScale {get;private set;}

  public event Action Changed;

  public event Action<string,bool> Completed;

  [DllImport(FitdPlayer.Library,CallingConvention=CallingConvention.Cdecl)]
  static extern void FitdUnity_FlushStartupSettings();
  public FitdSettingsClient(FitdPlayer owner){

   player=owner;Settings=JsonUtility.FromJson<FitdSettingsSchema>(Resources.Load<TextAsset>("FitdUI/SettingsSchema").text).settings;

   if(FitdUnity_SettingsCount()!=Settings.Length)throw new InvalidOperationException("Unity settings/plugin schema mismatch. Rebuild the native plugin.");

   Values=new float[Settings.Length];raw=new float[Settings.Length];LoadLocal();Refresh();

  }

  void LoadLocal(){Volume=PlayerPrefs.GetFloat("Fitd.Volume",1);ReduceMotion=PlayerPrefs.GetInt("Fitd.ReduceMotion",0)!=0;UiScale=PlayerPrefs.GetFloat("Fitd.UiScale",1);}

  public void SetVolume(float value){Volume=Mathf.Clamp01(value);player.SetAudioVolume(Volume);}

  public void SetMotion(bool reduce){ReduceMotion=reduce;}

  public void SetScale(float scale){UiScale=Mathf.Clamp(scale,.85f,1.25f);Changed?.Invoke();}

  public void Set(int id,float value){

   value=Mathf.Clamp(value,Settings[id].min,Settings[id].max);if(Settings[id].kind=="int" || Settings[id].kind=="key" || Settings[id].kind=="pad")value=Mathf.Round(value);

   pending[id]=value;Values[id]=value;FitdUnity_SetSetting(id,value);

   if(id==1)player.SetTextureFiltering(value>.5f);

   if(id==52)player.ApplyDisplayMode(value>.5f);

   Changed?.Invoke();

  }

  public void SetText(int id,string value){if(Encoding.UTF8.GetByteCount(value)>255)return;Text[id]=value;FitdUnity_SetSettingText(id,value);}

  public void Request(int type){if(Busy)return;action=type;if(type==0 && !Application.isEditor)Set(52,Screen.fullScreen?1:0);request=FitdUnity_SettingsAction(type);}

  public void Tick(){
   if(player.AwaitingStart)FitdUnity_FlushStartupSettings();

   double now=Time.realtimeSinceStartupAsDouble;if(now<nextPoll)return;nextPoll=now+.08;Refresh();

   if(request!=0 && FitdUnity_SettingsResult(out int status)>=request){

    int done=action;request=0;pending.Clear();Refresh(true);

    if(status==1){

     if(done==0){PlayerPrefs.SetFloat("Fitd.Volume",Volume);PlayerPrefs.SetInt("Fitd.ReduceMotion",ReduceMotion?1:0);PlayerPrefs.SetFloat("Fitd.UiScale",UiScale);PlayerPrefs.Save();}

     if(done==1)LoadLocal();

     if(done==2){Volume=1;ReduceMotion=false;UiScale=1;}

     if(done!=0)player.ApplyDisplayMode(Values[52]>.5f);

     player.SetAudioVolume(Volume);Changed?.Invoke();

    }

    Completed?.Invoke(status==1?(done==0?"Settings saved":done==1?"Saved settings restored":"Defaults restored — save to keep them"):"Could not save settings. Check the save directory.",status==1);

   }

  }

  void Refresh(bool force=false){

   int next=FitdUnity_GetSettings(raw,raw.Length);if(next==0 || (!force && next==revision))return;revision=next;

   for(int i=0;i<raw.Length;i++){if(pending.TryGetValue(i,out float desired) && Mathf.Abs(desired-raw[i])<.001f)pending.Remove(i);Values[i]=pending.TryGetValue(i,out desired)?desired:raw[i];}

   for(int i=0;i<2;i++){var buffer=new StringBuilder(256);FitdUnity_GetSettingText(i,buffer,256);Text[i]=buffer.ToString();}

   player.SetTextureFiltering(Values[1]>.5f);player.SetAudioVolume(Volume);Changed?.Invoke();

  }

 }

}

