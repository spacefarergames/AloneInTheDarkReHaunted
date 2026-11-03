using System;
using System.Runtime.InteropServices;
using Fitd.Unity;
using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine;
using UnityEngine.UIElements;

[InitializeOnLoad]
public static class FitdLaunchSettingsSmoke
{
    private const string Active = "FITD.LaunchTest";
    private static int phase;
    private static double deadline, captureReady;
    private static bool Jack => Array.IndexOf(Environment.GetCommandLineArgs(), "-fitdJack") >= 0;
    private static bool Skip => Array.IndexOf(Environment.GetCommandLineArgs(), "-fitdSkipLauncher") >= 0;

    [DllImport("FitdUnity")]
    private static extern void FitdUnity_GetSceneInfo(out int game, out int floor, out int camera, out int active);

    static FitdLaunchSettingsSmoke() { EditorApplication.update += Tick; }

    public static void Run()
    {
        PlayerPrefs.SetInt("Fitd.JackMode", 0);
        PlayerPrefs.SetInt("Fitd.SkipStartupSettings", Skip ? 1 : 0);
        PlayerPrefs.SetFloat("Fitd.RenderScale", 1);
        EditorSceneManager.OpenScene("Assets/FitdPlayer.unity");
        var view = Type.GetType("UnityEditor.GameView,UnityEditor");
        if (view != null) EditorWindow.GetWindow(view);
        SessionState.SetBool(Active, true);
        EditorApplication.isPlaying = true;
    }

    private static void Tick()
    {
        if (!SessionState.GetBool(Active, false) || !EditorApplication.isPlaying) return;
        if (deadline == 0) deadline = EditorApplication.timeSinceStartup + 180;
        try
        {
            if (EditorApplication.timeSinceStartup > deadline) throw new Exception("Launch test timed out at " + phase);
            var player = UnityEngine.Object.FindAnyObjectByType<FitdPlayer>();
            if (player == null || player.SettingsOverlay == null) return;
            if (player.Error != null) throw new Exception(player.Error);
            var overlay = player.SettingsOverlay;
            var root = overlay.GetComponent<UIDocument>().rootVisualElement;
            if (phase == 0)
            {
                if (Skip)
                {
                    if (player.AwaitingStart || overlay.BlocksGameplay) throw new Exception("Startup opt-out was ignored.");
                    phase = 1;
                    return;
                }
                if (!player.AwaitingStart || overlay.Progress < .99f) return;
                if (root.Q<Button>("save-settings").text != "Start Game" || root.Q("settings-launcher") != null)
                    throw new Exception("Incorrect startup controls");
                if (overlay.DisplayTexture(null)?.name != "Designer") throw new Exception("Wrong startup artwork");
                var mode = root.Q<DropdownField>("game-mode");
                if (mode.value != "AITD1" || !mode.enabledInHierarchy) throw new Exception("Game picker unavailable or wrong default");
                mode.value = Jack ? "JACK" : "AITD1";
                root.Q<Slider>("render-scale").value = .75f;
                root.Q<Toggle>("skip-startup-settings").value = true;
                ScreenCapture.CaptureScreenshot("D:/FITD/build/settings-captures/game-mode.png");
                captureReady = EditorApplication.timeSinceStartup + .35;
                phase = -1;
                return;
            }
            if (phase == -1)
            {
                if (EditorApplication.timeSinceStartup < captureReady) return;
                using (var submit = NavigationSubmitEvent.GetPooled())
                {
                    submit.target = root.Q<Button>("save-settings");
                    submit.target.SendEvent(submit);
                }
                phase = 1;
                return;
            }
            if (player.RenderedFrames < 10) return;
            FitdUnity_GetSceneInfo(out int game, out int floor, out _, out int active);
            if (game != (Jack ? 1 : 0)) throw new Exception("Unexpected native game ID " + game);
            if (Jack && (active == 0 || floor != 16)) return;
            if (phase == 1)
            {
                if (!Skip && (player.Frame.width != 720 || player.Frame.height != 450))
                    throw new Exception("Startup render scale was not applied");
                if (!Skip && PlayerPrefs.GetInt("Fitd.SkipStartupSettings") != 1)
                    throw new Exception("Startup opt-out was not saved");
                overlay.Open();
                overlay.SelectCategory("Display");
                root.Q<Slider>("render-scale").value = 1.5f;
                phase = 2;
                return;
            }
            if (player.Frame.width != 1440 || player.Frame.height != 900) return;
            Debug.Log($"FITD_LAUNCH_PASS game={game} floor={floor} active={active} resized={player.Frame.width}x{player.Frame.height} skip={Skip}");
            Finish(0);
        }
        catch (Exception e) { Debug.LogException(e); Finish(1); }
    }

    private static void Finish(int code)
    {
        PlayerPrefs.DeleteKey("Fitd.JackMode");
        PlayerPrefs.DeleteKey("Fitd.SkipStartupSettings");
        PlayerPrefs.DeleteKey("Fitd.RenderScale");
        PlayerPrefs.Save();
        SessionState.SetBool(Active, false);
        EditorApplication.isPlaying = false;
        EditorApplication.Exit(code);
    }
}
