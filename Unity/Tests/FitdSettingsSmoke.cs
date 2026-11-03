using System;
using System.IO;
using System.Runtime.InteropServices;
using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine;
using UnityEngine.UIElements;
[InitializeOnLoad] public static class FitdSettingsSmoke {
 const string Active="FITD.SettingsTest";static double next;static int phase;static float original;static int screenshots;
 [DllImport("FitdUnity")]static extern int FitdUnity_GetSettings([Out]float[] values,int count);
 static FitdSettingsSmoke(){EditorApplication.update+=Tick;}
 public static void Run(){
  var view=Type.GetType("UnityEditor.GameView,UnityEditor");if(view!=null)EditorWindow.GetWindow(view);
  EditorSceneManager.OpenScene("Assets/FitdPlayer.unity");SessionState.SetBool(Active,true);SessionState.SetFloat(Active+"Start",(float)EditorApplication.timeSinceStartup);EditorApplication.isPlaying=true;
 }
 static void Assert(bool ok,string message){if(!ok)throw new Exception(message);}
 static void Tick(){if(!SessionState.GetBool(Active,false))return;
  try{
   if(EditorApplication.timeSinceStartup-SessionState.GetFloat(Active+"Start",0)>180)throw new Exception("Settings test timed out at phase "+phase);
   if(!EditorApplication.isPlaying)return;
   var player=UnityEngine.Object.FindFirstObjectByType<Fitd.Unity.FitdPlayer>();if(player==null)return;if(player.Error!=null)throw new Exception(player.Error);
   var ui=player.SettingsOverlay;if(ui==null || !ui.Client.Ready || EditorApplication.timeSinceStartup<next)return;var client=ui.Client;var root=ui.GetComponent<UIDocument>().rootVisualElement;
   float[] values=new float[73];FitdUnity_GetSettings(values,73);
   switch(phase){
    case 0:ui.Open();next=EditorApplication.timeSinceStartup+2;phase++;break;
    case 1:
     Assert(ui.Progress>.99f && ui.BlocksGameplay,"Overlay did not open");Assert(root.Q("settings-card").worldBound.width>300,"Card has no layout");
     Directory.CreateDirectory("D:/FITD/build/settings-captures");ScreenCapture.CaptureScreenshot("D:/FITD/build/settings-captures/display.png");
     original=values[7];ui.SelectCategory("Lighting");root.Q<Toggle>("control-7").value=original<.5f;phase++;next=EditorApplication.timeSinceStartup+.5;break;
    case 2:Assert(values[7]!=(original),"UI toggle did not reach gameplay config");client.Set(7,original);client.Set(8,.77f);phase++;next=EditorApplication.timeSinceStartup+.5;break;
    case 3:Assert(Mathf.Abs(values[8]-.77f)<.001f,"Slider value did not reach config");client.Request(0);phase++;next=EditorApplication.timeSinceStartup+.5;break;
    case 4:if(client.Busy)return;client.Set(8,.55f);client.Request(1);phase++;next=EditorApplication.timeSinceStartup+.5;break;
    case 5:if(client.Busy)return;Assert(Mathf.Abs(values[8]-.77f)<.001f,"Reload did not restore saved settings");ui.SelectCategory("Controls");client.Set(63,0);client.Set(53,26);phase++;next=EditorApplication.timeSinceStartup+.5;break;
    case 6:Assert(values[63]==0 && values[53]==26,"Bindings not applied");ScreenCapture.CaptureScreenshot("D:/FITD/build/settings-captures/controls.png");client.Request(2);phase++;next=EditorApplication.timeSinceStartup+.5;break;
    case 7:if(client.Busy)return;Assert(Mathf.Abs(values[8]-.77f)>.001f,"Restore defaults failed");ui.SelectCategory("Interface");ScreenCapture.CaptureScreenshot("D:/FITD/build/settings-captures/interface.png");ui.Close();phase++;next=EditorApplication.timeSinceStartup+1;break;
    case 8:Assert(ui.Progress==0 && !ui.BlocksGameplay,"Overlay did not finish closing");Assert(Mathf.Abs(ui.GameRect().width-Screen.width)<1,"Game did not return to full screen");ui.Open();phase++;next=EditorApplication.timeSinceStartup+.08;break;
    case 9:ui.Close();phase++;next=EditorApplication.timeSinceStartup+.08;break;
    case 10:ui.Open();phase++;next=EditorApplication.timeSinceStartup+1;break;
    case 11:Assert(ui.Progress>.99f,"Animation reversal failed");client.Request(1);phase++;next=EditorApplication.timeSinceStartup+.5;break;
    case 12:if(client.Busy)return;ui.Close();Debug.Log("FITD_SETTINGS_PASS: UI toggle, slider, save/reload/defaults, bindings, full-screen restore, animation reversal; captures="+Screen.width+"x"+Screen.height);SessionState.SetBool(Active,false);EditorApplication.isPlaying=false;EditorApplication.Exit(0);break;
   }
  }catch(Exception e){Debug.LogException(e);SessionState.SetBool(Active,false);EditorApplication.Exit(1);}
 }
}
