using System;
using System.IO;
using UnityEditor;
using UnityEditor.Build.Reporting;
public static class FitdBuildPlayerSmoke
{
    public static void RunLinux()
    {
        if (EditorUserBuildSettings.activeBuildTarget != BuildTarget.StandaloneLinux64)
            throw new InvalidOperationException("Launch Unity with -buildTarget Linux64.");
        Fitd.Unity.FitdBundleBuilder.BuildImported("aitd1");
        Fitd.Unity.FitdBundleBuilder.BuildImported("jack");
        var args = Environment.GetCommandLineArgs();
        int flag = Array.IndexOf(args, "-fitdOutput");
        string output = Path.GetFullPath(flag >= 0 ? args[flag + 1] : "../../build/UnityStandalone/Linux/TatouUnity.x86_64");
        Directory.CreateDirectory(Path.GetDirectoryName(output));
        var report = BuildPipeline.BuildPlayer(new[]{"Assets/FitdPlayer.unity"}, output,
            BuildTarget.StandaloneLinux64, BuildOptions.None);
        if (report.summary.result != BuildResult.Succeeded)
            throw new Exception("FITD Linux player build failed: " + report.summary.result);
        UnityEngine.Debug.Log("FITD_LINUX_BUILD_PASS: " + output);
    }

    public static void Run()
    {
        var args=Environment.GetCommandLineArgs();int flag=Array.IndexOf(args,"-fitdOutput");
        string output=flag>=0?Path.GetFullPath(args[flag+1]):Path.GetFullPath("../../build/UnityStandalone/Windows/TatouUnity.exe");
        Directory.CreateDirectory(Path.GetDirectoryName(output));
        var scene=UnityEditor.SceneManagement.EditorSceneManager.OpenScene("Assets/FitdPlayer.unity");
        if(UnityEngine.Camera.main!=null){UnityEngine.Camera.main.clearFlags=UnityEngine.CameraClearFlags.SolidColor;UnityEngine.Camera.main.backgroundColor=UnityEngine.Color.black;}
        UnityEditor.SceneManagement.EditorSceneManager.SaveScene(scene);
        PlayerSettings.productName="TatouUnity";
        PlayerSettings.companyName="Spacefarer Retro Remasters";
        PlayerSettings.SplashScreen.show=false;
        PlayerSettings.SplashScreen.showUnityLogo=false;
        const string iconFolder="Assets/Editor/TatouIcons";
        var largest=AssetDatabase.LoadAssetAtPath<UnityEngine.Texture2D>(iconFolder+"/Tatou-256.png");
        if(largest!=null)
        {
            var sizes=PlayerSettings.GetIconSizesForTargetGroup(BuildTargetGroup.Standalone);
            var icons=Array.ConvertAll(sizes,size=>AssetDatabase.LoadAssetAtPath<UnityEngine.Texture2D>(iconFolder+"/Tatou-"+size+".png") ?? largest);
            PlayerSettings.SetIconsForTargetGroup(BuildTargetGroup.Standalone,icons);
            PlayerSettings.SetIconsForTargetGroup(BuildTargetGroup.Unknown,new[]{largest});
        }
        PlayerSettings.fullScreenMode=UnityEngine.FullScreenMode.FullScreenWindow;
        PlayerSettings.defaultIsNativeResolution=true;
        var report=BuildPipeline.BuildPlayer(new[]{"Assets/FitdPlayer.unity"},output,BuildTarget.StandaloneWindows64,BuildOptions.None);
        if(report.summary.result!=BuildResult.Succeeded)throw new Exception("FITD player build failed: "+report.summary.result);
        int runtimeFlag=Array.IndexOf(args,"-fitdVcRuntime");
        if(runtimeFlag>=0)
        {
            string directory=Path.GetFullPath(args[runtimeFlag+1]);
            foreach(string name in new[]{"msvcp140.dll","vcruntime140.dll","vcruntime140_1.dll"})
                File.Copy(Path.Combine(directory,name),Path.Combine(Path.GetDirectoryName(output),name),true);
        }
        UnityEngine.Debug.Log("FITD_PLAYER_BUILD_PASS: "+output+" bytes="+report.summary.totalSize);
    }
}
