using System;
using System.Runtime.InteropServices;
using System.Reflection;
using UnityEngine;
using UnityEngine.InputSystem;
using UnityEngine.InputSystem.LowLevel;
public static class FitdInputSmoke {
 [DllImport("FitdUnity",CallingConvention=CallingConvention.Cdecl)] static extern void FitdUnity_GetInputStatus([Out]int[] state,int count);
 static Keyboard keyboard;static Gamepad pad;static int stage=-1,sequence;static uint frame;static double started;
 static readonly int[] status=new int[5];
 static Action<bool> poll;
 static InputSettings.BackgroundBehavior previousBackground;
#if UNITY_EDITOR
 static InputSettings.EditorInputBehaviorInPlayMode previousEditorInput;
#endif
 static void Cleanup(){
  poll?.Invoke(false);
  if(keyboard!=null)InputSystem.RemoveDevice(keyboard);if(pad!=null)InputSystem.RemoveDevice(pad);
  InputSystem.settings.backgroundBehavior=previousBackground;
#if UNITY_EDITOR
  InputSystem.settings.editorInputBehaviorInPlayMode=previousEditorInput;
#endif
 }
 public static void Fail(string error){Cleanup();Error=error;}
 public static bool Done;public static string Error;
 public static void Start(Fitd.Unity.FitdPlayer player){var runner=player.gameObject.AddComponent<FitdInputSmokeRunner>();runner.player=player;}
 public static bool Tick(Fitd.Unity.FitdPlayer player){
  if(stage<0){
   previousBackground=InputSystem.settings.backgroundBehavior;InputSystem.settings.backgroundBehavior=InputSettings.BackgroundBehavior.IgnoreFocus;
#if UNITY_EDITOR
   previousEditorInput=InputSystem.settings.editorInputBehaviorInPlayMode;InputSystem.settings.editorInputBehaviorInPlayMode=InputSettings.EditorInputBehaviorInPlayMode.AllDeviceInputAlwaysGoesToGameView;
#endif
   keyboard=InputSystem.AddDevice<Keyboard>();pad=InputSystem.AddDevice<Gamepad>();player.automaticInput=false;
   var bridge=typeof(Fitd.Unity.FitdPlayer).GetField("devices",BindingFlags.Instance|BindingFlags.NonPublic).GetValue(player);
   poll=(Action<bool>)Delegate.CreateDelegate(typeof(Action<bool>),bridge,bridge.GetType().GetMethod("Poll"));
   stage=0;Send(player);return false;
  }
  // Virtual-device forwarding is independent of OS focus in automated/hidden runs.
  // Production focus gating remains enabled in FitdPlayer.Update.
  poll(true);player.SubmitInput(0,0,false,new Vector2(-1,-1));
  FitdUnity_GetInputStatus(status,5);
  if(Time.realtimeSinceStartupAsDouble-started>10)throw new Exception("Input forwarding timed out at stage "+stage+": "+string.Join(",",status));
  if(player.RenderedFrames-frame<3 || status[4]-sequence<2)return false;
  int joy=stage==1?65:stage==3?8:stage==4?1:stage==5?20:0;
  int key=stage==2 || stage==4?0x1c:0,click=stage==1 || stage==3?1:0,connected=stage>=3 && stage<=5?1:0;
  // A virtual, idle gamepad remains connected for all stages.
  connected=1;
  if(status[0]!=joy || status[1]!=key || status[2]!=click || status[3]!=connected)throw new Exception("Input stage "+stage+" expected "+joy+","+key+","+click+","+connected+" got "+string.Join(",",status));
  if(++stage>6){Cleanup();player.automaticInput=false;player.SubmitInput(0,0,false,new Vector2(-1,-1));Debug.Log("FITD_INPUT_PASS: Unity keyboard arrows/run/action/confirm and gamepad stick/buttons/D-pad/release reach native gameplay");return true;}
  Send(player);return false;
 }
 static void Send(Fitd.Unity.FitdPlayer player){
  var keys=stage==1?new KeyboardState(Key.UpArrow,Key.LeftShift,Key.Space):stage==2?new KeyboardState(Key.Enter):new KeyboardState();
  var state=new GamepadState();
  if(stage==3)state=new GamepadState{leftStick=new Vector2(1,0)}.WithButton(GamepadButton.South);
  if(stage==4)state=new GamepadState{leftStick=new Vector2(0,1)}.WithButton(GamepadButton.Start);
  if(stage==5)state=new GamepadState().WithButton(GamepadButton.DpadLeft).WithButton(GamepadButton.LeftShoulder);
  InputSystem.QueueStateEvent(keyboard,keys);InputSystem.QueueStateEvent(pad,state);keyboard.MakeCurrent();pad.MakeCurrent();
  FitdUnity_GetInputStatus(status,5);sequence=status[4];frame=player.RenderedFrames;started=Time.realtimeSinceStartupAsDouble;
 }
}

public sealed class FitdInputSmokeRunner:MonoBehaviour {
 public Fitd.Unity.FitdPlayer player;
 void LateUpdate(){try{if(FitdInputSmoke.Tick(player)){FitdInputSmoke.Done=true;Destroy(this);}}catch(Exception error){FitdInputSmoke.Fail(error.ToString());Destroy(this);}}
}
