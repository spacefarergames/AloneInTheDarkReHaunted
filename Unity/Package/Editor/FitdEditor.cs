using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine;

namespace Fitd.Unity
{
    [InitializeOnLoad]
    public static class FitdEditor
    {
        static FitdEditor()
        {
            EditorApplication.update += Pump;
            EditorApplication.playModeStateChanged += transition => {
                if (transition == PlayModeStateChange.ExitingEditMode && Resources.FindObjectsOfTypeAll<FitdGpuState>().Length == 0)
                {
                    // Allocate the state in Edit mode so Unity does not roll it
                    // back with objects created by the temporary Play-mode scene.
                    var state = ScriptableObject.CreateInstance<FitdGpuState>();
                    state.hideFlags = HideFlags.HideAndDontSave;
                }
            };
            EditorApplication.pauseStateChanged += pause => {
                if (EditorApplication.isPlaying) FitdPlayer.FitdUnity_SetPaused(pause == PauseState.Paused ? 1 : 0);
            };
            EditorApplication.quitting += () => {
                try { FitdPlayer.FitdUnity_Stop(); }
                catch (System.DllNotFoundException) { }
                catch (System.EntryPointNotFoundException) { }
            };
        }

        private static double lastPump,lastWindowScan;
        private static EditorWindow[] gameViews=System.Array.Empty<EditorWindow>();
        private static void Pump()
        {
            if(!EditorApplication.isPlaying || EditorApplication.isPaused || !FitdPlayer.HasPlayer){lastPump=0;return;}
            double now=EditorApplication.timeSinceStartup;
            if(now-lastPump<1.0/FitdPlayer.RequestedFrameRate)return;
            lastPump=now;
            // Editor Update can be throttled independently of MonoBehaviour.Update
            // when another tab owns focus. Drive the bridge from the editor clock.
            FitdPlayer.PumpEditorFrame();EditorApplication.QueuePlayerLoopUpdate();
            if(now-lastWindowScan>1){lastWindowScan=now;gameViews=System.Array.FindAll(Resources.FindObjectsOfTypeAll<EditorWindow>(),window=>window.GetType().FullName=="UnityEditor.GameView");}
            foreach(var window in gameViews)if(window!=null)window.Repaint();
        }

        [MenuItem("FITD/Create Player Scene")]
        public static void CreateScene()
        {
            if (!EditorSceneManager.SaveCurrentModifiedScenesIfUserWantsTo()) return;
            var scene = EditorSceneManager.NewScene(NewSceneSetup.EmptyScene, NewSceneMode.Single);
            var camera=new GameObject("Main Camera",typeof(Camera),typeof(AudioListener));camera.tag="MainCamera";
            camera.GetComponent<Camera>().clearFlags=CameraClearFlags.SolidColor;
            camera.GetComponent<Camera>().backgroundColor=Color.black;
            new GameObject("FITD Native Player").AddComponent<FitdPlayer>();
            EditorSceneManager.MarkSceneDirty(scene);
        }
    }
}
