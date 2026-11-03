using System;
using System.Reflection;
using UnityEngine;
public static class FitdPresentationSmoke {
 public static void Check(Fitd.Unity.FitdPlayer player){
  var camera=Camera.main;if(camera==null || camera.clearFlags!=CameraClearFlags.SolidColor || camera.backgroundColor!=Color.black)throw new Exception("Main camera must clear solid black");
  var field=typeof(Fitd.Unity.FitdPlayer).GetField("viewport",BindingFlags.Instance|BindingFlags.NonPublic);var viewport=(Rect)field.GetValue(player);
  if(viewport!=new Rect(0,0,Screen.width,Screen.height))throw new Exception("Game viewport does not fill the screen: "+viewport+" screen="+Screen.width+"x"+Screen.height);
  Debug.Log("FITD_PRESENTATION_PASS: full-screen game viewport and solid-black camera");
 }
}
