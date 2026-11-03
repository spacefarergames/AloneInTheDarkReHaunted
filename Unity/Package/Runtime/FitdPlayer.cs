using System;
using System.Collections;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;
using UnityEngine;
using Unity.Profiling;
#if ENABLE_INPUT_SYSTEM
using UnityEngine.InputSystem;
#endif

namespace Fitd.Unity
{
    // One engine session per process. Leaving Play mode pauses it; entering resumes.
    // No managed delegates are retained by the native engine across domain reloads.
    public sealed class FitdPlayer : MonoBehaviour
    {
        [Range(320, 4096)] public int renderWidth = 960;
        [Range(200, 4096)] public int renderHeight = 600;
        public bool useAssetBundles = true;
        public bool automaticInput = true;
        [Range(15,240)] public int targetFrameRate = 60;
        public static bool SessionAttached => owner!=null && owner.attached;
        public static bool HasPlayer => owner!=null;
        public static int RequestedFrameRate => owner!=null?Mathf.Clamp(owner.targetFrameRate,15,240):60;
        private static readonly ProfilerMarker PumpMarker=new ProfilerMarker("FITD.Pump");
        private static readonly ProfilerMarker CopyMarker=new ProfilerMarker("FITD.CopyPacket");
        private static readonly ProfilerMarker InputMarker=new ProfilerMarker("FITD.Input");
        private double nextPumpTime;
        private int previousVsync,previousRenderInterval,previousTarget;
        private bool pacingConfigured,previousBackground;
#if ENABLE_INPUT_SYSTEM
        private float previousPollingFrequency;
#endif
        public static bool PumpEditorFrame()
        {
            if(!Application.isEditor || !Application.isPlaying || owner==null)return false;
            uint previous=owner.RenderedFrames;owner.PumpFrame();return owner.RenderedFrames!=previous;
        }
        public Texture Frame { get; private set; }
        public string Error { get; private set; }
        public float LoadingProgress { get; private set; }
        public double RendererCpuMilliseconds => gpu?.LastFrameMilliseconds ?? 0;
        public double RendererTotalCpuMilliseconds => gpu?.TotalFrameMilliseconds ?? 0;
        public int RendererDrawCalls => gpu?.LastDrawCalls ?? 0;
        public uint RenderedFrames { get; private set; }
        private byte[] pixels;
        private GCHandle packetPin;
        private ulong serial;
        private bool attached;
        private bool closing;
        private bool optionsHeld;
        private static FitdPlayer owner;
        private Rect viewport;
        private int abi;
        private FitdGpuRenderer gpu;
        public FitdSettingsOverlay SettingsOverlay {get;private set;}
        public bool AwaitingStart { get; private set; }
        public bool CanSelectGame => useAssetBundles && System.IO.File.Exists(System.IO.Path.Combine(
            Application.streamingAssetsPath, "Fitd", FitdBundleStore.PlatformName, "jack", "fitd-catalog"));
        public bool JackSelected { get; set; }
        public bool SkipStartupSettings { get; set; }
        public float RenderScale { get; private set; } = 1;
        public bool HasEngine => attached;
        public Vector2Int RenderSize => new Vector2Int(
            Mathf.Clamp(Mathf.RoundToInt(renderWidth * RenderScale), 320, 4096),
            Mathf.Clamp(Mathf.RoundToInt(renderHeight * RenderScale), 200, 4096));
        public void SetRenderScale(float scale)
        {
            RenderScale = Mathf.Clamp(scale, .5f, 2f);
            if (attached) FitdUnity_SetRenderSize(RenderSize.x, RenderSize.y);
            if (!Application.isEditor && !Screen.fullScreen) ApplyDisplayMode(false);
        }
        private Coroutine windowResize;

        public void ApplyDisplayMode(bool fullscreen)
        {
            if (Application.isEditor) return;
            if (windowResize != null)
            {
                StopCoroutine(windowResize);
                windowResize = null;
            }
            if (fullscreen)
            {
                Screen.SetResolution(Display.main.systemWidth, Display.main.systemHeight, FullScreenMode.FullScreenWindow);
            }
            else
            {
                Screen.fullScreen = false;
                windowResize = StartCoroutine(ResizeWindow());
            }
        }

        private IEnumerator ResizeWindow()
        {
            // Unity commits the display mode at the end of the frame.
            for (int frame = 0; frame < 8 && Screen.fullScreen; frame++)
                yield return null;
            yield return null;
            var size = RenderSize;
            Screen.SetResolution(size.x, size.y, FullScreenMode.Windowed);
            yield return null;
            yield return null;
#if UNITY_STANDALONE_WIN && !UNITY_EDITOR
            if (!Screen.fullScreen && (Screen.width != size.x || Screen.height != size.y))
                FitdWindowsWindow.ResizeClient(size.x, size.y);
#endif
            windowResize = null;
        }

        public void LoadHostSettings(bool defaults)
        {
            JackSelected = !defaults && CanSelectGame && PlayerPrefs.GetInt("Fitd.JackMode", 0) != 0;
            SkipStartupSettings = !defaults && PlayerPrefs.GetInt("Fitd.SkipStartupSettings", 0) != 0;
            SetRenderScale(defaults ? 1 : PlayerPrefs.GetFloat("Fitd.RenderScale", 1));
        }
        public void SaveHostSettings()
        {
            PlayerPrefs.SetInt("Fitd.JackMode", JackSelected ? 1 : 0);
            PlayerPrefs.SetInt("Fitd.SkipStartupSettings", SkipStartupSettings ? 1 : 0);
            PlayerPrefs.SetFloat("Fitd.RenderScale", RenderScale);
            PlayerPrefs.Save();
        }
        public void StartGame()
        {
            if (!AwaitingStart) return;
            SaveHostSettings();
            AwaitingStart = false;
        }
        public void SetTextureFiltering(bool enabled){gpu?.SetTextureFiltering(enabled);}
        public void SetAudioVolume(float volume){var source=GetComponent<AudioSource>();if(source!=null)source.volume=volume;}
        private Texture2D legacyFrame;
        private FitdBundleStore content;
        private FitdMemoryContent memoryContent;
        private static bool registeredShutdown;
        private readonly FitdInputBridge devices = new FitdInputBridge();

#if UNITY_IOS && !UNITY_EDITOR
        internal const string Library = "__Internal";
#else
        internal const string Library = "FitdUnity";
#endif
        [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int FitdUnity_GetAbiVersion();
        [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int FitdUnity_GetState();
        [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] private static extern int FitdUnity_Start(int width, int height);
        [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] private static extern void FitdUnity_ToggleOptions();
        [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] private static extern void FitdUnity_ConfigureAudio(uint sampleRate);
        [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] private static extern int FitdUnity_CopyGpuPacket(IntPtr buffer, int capacity);
        [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] private static extern void FitdUnity_AcknowledgeGpuPacket(uint frame);
        [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] private static extern int FitdUnity_SetContentPaths([MarshalAs(UnmanagedType.LPUTF8Str)] string contentRoot, [MarshalAs(UnmanagedType.LPUTF8Str)] string saveRoot);
        [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] public static extern void FitdUnity_SetPaused(int paused);
        [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] public static extern void FitdUnity_Stop();
        [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] private static extern void FitdUnity_GetError(StringBuilder buffer, int capacity);
        [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] private static extern void FitdUnity_SetInput(int joy, int key, int click, float x, float y, int buttons);
        [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] private static extern int FitdUnity_CopyFrame([Out] byte[] buffer, int capacity, out int width, out int height, ref ulong serial);

        [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] private static extern void FitdUnity_UseManagedStartup();
        [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] private static extern void FitdUnity_SetRenderSize(int width, int height);

        [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
        private static extern void FitdUnity_PrepareSettings([MarshalAs(UnmanagedType.LPUTF8Str)] string path);

        private void EnsureSettingsOverlay()
        {
            SettingsOverlay = GetComponent<FitdSettingsOverlay>();
            if (SettingsOverlay == null) SettingsOverlay = gameObject.AddComponent<FitdSettingsOverlay>();
            SettingsOverlay.Initialize(this);
        }

        private void OnEnable()
        {
            if (!Application.isPlaying) return;
            if (owner != null && owner != this) { Error = "Only one FITD player can own the native engine."; return; }
            owner = this;closing=false;
            var camera=Camera.main;
            if(camera!=null){camera.clearFlags=CameraClearFlags.SolidColor;camera.backgroundColor=Color.black;}
            previousVsync=QualitySettings.vSyncCount;previousRenderInterval=UnityEngine.Rendering.OnDemandRendering.renderFrameInterval;
            previousTarget=Application.targetFrameRate;previousBackground=Application.runInBackground;pacingConfigured=true;
#if ENABLE_INPUT_SYSTEM
            previousPollingFrequency=InputSystem.pollingFrequency;InputSystem.pollingFrequency=Mathf.Max(120,previousPollingFrequency);
#endif
            QualitySettings.vSyncCount=0;UnityEngine.Rendering.OnDemandRendering.renderFrameInterval=1;
            Application.runInBackground=true;Application.targetFrameRate=Mathf.Clamp(targetFrameRate,15,240);
            Error = null; RenderedFrames = 0; LoadingProgress = 0;nextPumpTime=0;
            StartCoroutine(GuardStartup(StartSession()));
        }

        private IEnumerator StartSession()
        {
            abi = FitdUnity_GetAbiVersion();
            if (abi != 1 && abi != 2) throw new InvalidOperationException("FITD native plugin ABI mismatch.");
            if(!registeredShutdown){Application.quitting+=()=>{if(!Application.isEditor)FitdUnity_Stop();};registeredShutdown=true;}
            if (abi == 2)
            {
                LoadHostSettings(false);
                bool managedStartup = useAssetBundles && !Array.Exists(Environment.GetCommandLineArgs(), a => a == "-fitdSkipManagedStartup");
                if (managedStartup)
                {
                    FitdUnity_UseManagedStartup();
                    if (FitdUnity_GetState() == 0)
                    {
                        string directory = System.IO.Path.Combine(Application.persistentDataPath, "FitdSaves", "aitd1");
                        System.IO.Directory.CreateDirectory(directory);
                        FitdUnity_PrepareSettings(System.IO.Path.Combine(directory, "aitd_remaster.cfg"));
                    }
                }
                if (managedStartup && FitdUnity_GetState() == 0 && !SkipStartupSettings)
                {
                    AwaitingStart = true;
                    EnsureSettingsOverlay();
                    yield return new WaitUntil(() => !AwaitingStart && !SettingsOverlay.BlocksGameplay);
                }
            }
            if(abi==2 && useAssetBundles)
            {
                content = new FitdBundleStore();
                string bundleRoot = JackSelected
                    ? System.IO.Path.Combine(Application.streamingAssetsPath, "Fitd", FitdBundleStore.PlatformName, "jack")
                    : null;
                yield return content.Initialize(bundleRoot);
                bool register = FitdUnity_GetState() == 0;
                if (register)
                {
                    string game = FitdBundleStore.NormalizeKey(content.Catalog.game);
                    if (game.Contains("/")) throw new InvalidOperationException("Invalid bundled game ID.");
                    string saves = System.IO.Path.Combine(Application.persistentDataPath, "FitdSaves", game);
                    if (FitdUnity_SetContentPaths(Application.streamingAssetsPath, saves) == 0)
                        throw new InvalidOperationException("Could not configure game save paths.");
                }
                memoryContent = new FitdMemoryContent(content);
                yield return memoryContent.Prepare(register);
            }
            LoadingProgress=1;
            BeginEngine();
        }

        private IEnumerator GuardStartup(IEnumerator startup)
        {
            var pending=new Stack<IEnumerator>();pending.Push(startup);
            try {
                while(pending.Count!=0)
                {
                    object next=null;bool advanced=false;Exception failure=null;
                    try {advanced=pending.Peek().MoveNext();if(advanced)next=pending.Peek().Current;}
                    catch(Exception error){failure=error;}
                    if(failure!=null){Error=failure.Message;Debug.LogException(failure,this);ShutdownHost(1);yield break;}
                    if(!advanced){(pending.Pop() as IDisposable)?.Dispose();continue;}
                    var child=next as IEnumerator;
                    if(child!=null)pending.Push(child);else yield return next;
                }
            }finally{while(pending.Count!=0)(pending.Pop() as IDisposable)?.Dispose();}
        }

        private void BeginEngine()
        {
            try
            {
                if (abi == 2)
                {
                    FitdAudioOutput.ConfigureLowLatency();
                    FitdUnity_ConfigureAudio((uint)AudioSettings.outputSampleRate);
                    gpu = new FitdGpuRenderer(RenderSize.x, RenderSize.y); Frame = gpu.Frame;
                }
                if (FitdUnity_Start(abi == 2 ? RenderSize.x : renderWidth, abi == 2 ? RenderSize.y : renderHeight) == 0) { ReadError(); ShutdownHost(1); return; }
                attached = true;
                if (abi == 2)
                {
                    var output = GetComponent<FitdAudioOutput>();
                    if (output == null) output = gameObject.AddComponent<FitdAudioOutput>();
                    output.Begin(AudioSettings.outputSampleRate);
                    if (SettingsOverlay == null) EnsureSettingsOverlay();
                    else SettingsOverlay.EngineStarted();
                }
            }
            catch (Exception e) { Error = e.Message; Debug.LogError(Error, this); ShutdownHost(1); }
        }

        private void ReadError()
        {
            var message = new StringBuilder(1024);
            FitdUnity_GetError(message, message.Capacity);
            Error = message.Length > 0 ? message.ToString() : "The engine has stopped. Restart Unity to begin a fresh engine session.";
            Debug.LogError(Error, this);
        }

        private void Update(){
            // Read device state in the player loop, after Input System's dynamic
            // update. Editor callbacks use a separate input-state buffer.
            if(attached && automaticInput){using(InputMarker.Auto())SendInput();}
            PumpFrame();
        }
        private void PumpFrame()
        {
            // The normal player loop and Editor fallback share one deadline.
            // A slow Editor callback must never disable regular Play-mode updates.
            double now=(double)System.Diagnostics.Stopwatch.GetTimestamp()/System.Diagnostics.Stopwatch.Frequency;
            if(now<nextPumpTime)return;
            double interval=1.0/Mathf.Clamp(targetFrameRate,15,240);
            uint previousFrame=RenderedFrames;
            try { using(PumpMarker.Auto())UpdateEngine();
                // An empty queue must not consume a presentation deadline.
                if(RenderedFrames!=previousFrame)nextPumpTime=Math.Max(nextPumpTime+interval,now+interval*0.1);
            }
            catch (Exception error)
            {
                Error = error.Message; Debug.LogException(error, this);
                ShutdownHost(1);
            }
        }

        private void UpdateEngine()
        {
            if (!attached) return;
            int state = FitdUnity_GetState();
            if(state==-1){ReadError();ShutdownHost(1);return;}
            if(state==3){ShutdownHost(0);return;}
            memoryContent?.Pump();
            if (abi == 2)
            {
                int size;using(CopyMarker.Auto())size=FitdUnity_CopyGpuPacket(packetPin.IsAllocated?packetPin.AddrOfPinnedObject():IntPtr.Zero,packetPin.IsAllocated?pixels.Length:0);
                if (size < 0)
                {
                    if (size == int.MinValue || -size > 256*1024*1024) throw new InvalidOperationException("Native render packet exceeds its size limit.");
                    if(packetPin.IsAllocated)packetPin.Free();
                    pixels = new byte[-size];packetPin=GCHandle.Alloc(pixels,GCHandleType.Pinned);
                    using(CopyMarker.Auto())size=FitdUnity_CopyGpuPacket(packetPin.AddrOfPinnedObject(),pixels.Length);
                }
                if (size > 0) { gpu.Render(pixels, size); FitdUnity_AcknowledgeGpuPacket(gpu.FrameNumber); RenderedFrames++; }
                return;
            }
            int w, h;
            int copied = FitdUnity_CopyFrame(pixels, pixels == null ? 0 : pixels.Length, out w, out h, ref serial);
            if (copied == -2)
            {
                pixels = new byte[checked(w * h * 4)];
                if (Frame != null) Destroy(Frame);
                legacyFrame = new Texture2D(w, h, TextureFormat.RGBA32, false, false) { name = "FITD native frame" }; Frame = legacyFrame;
                copied = FitdUnity_CopyFrame(pixels, pixels.Length, out w, out h, ref serial);
            }
            if (copied > 0) { legacyFrame.LoadRawTextureData(pixels); legacyFrame.Apply(false, false); RenderedFrames++; }
        }

#if ENABLE_INPUT_SYSTEM
        private static bool Pressed(UnityEngine.InputSystem.Controls.ButtonControl control)=>control!=null && (control.isPressed || control.wasPressedThisFrame);
#endif
        private static bool Held(KeyCode key)
        {
#if ENABLE_INPUT_SYSTEM
            var keyboard = Keyboard.current;
            if (keyboard == null) return false;
            switch (key)
            {
                case KeyCode.W: return Pressed(keyboard.wKey);
                case KeyCode.S: return Pressed(keyboard.sKey);
                case KeyCode.A: return Pressed(keyboard.aKey);
                case KeyCode.D: return Pressed(keyboard.dKey);
                case KeyCode.Q: return Pressed(keyboard.qKey);
                case KeyCode.E: return Pressed(keyboard.eKey);
                case KeyCode.UpArrow: return Pressed(keyboard.upArrowKey);
                case KeyCode.DownArrow: return Pressed(keyboard.downArrowKey);
                case KeyCode.LeftArrow: return Pressed(keyboard.leftArrowKey);
                case KeyCode.RightArrow: return Pressed(keyboard.rightArrowKey);
                case KeyCode.LeftShift: return Pressed(keyboard.leftShiftKey);
                case KeyCode.Space: return Pressed(keyboard.spaceKey);
                case KeyCode.Return: return Pressed(keyboard.enterKey);
                case KeyCode.Escape: return Pressed(keyboard.escapeKey);
                case KeyCode.Tab: return Pressed(keyboard.tabKey);
                case KeyCode.F1: return Pressed(keyboard.f1Key);
                case KeyCode.Home: return Pressed(keyboard.homeKey);
                default: return false;
            }
#else
            return Input.GetKey(key);
#endif
        }

        private void SendInput()
        {
            
            if (!Application.isFocused) { if(abi==2)devices.Clear();FitdUnity_SetInput(0, 0, 0, -1, -1, 0); return; }
            bool options = Held(KeyCode.F1) || Held(KeyCode.Home);
            if(options && !optionsHeld && !(SettingsOverlay?.CapturingKey??false)){if(SettingsOverlay!=null)SettingsOverlay.Toggle();else FitdUnity_ToggleOptions();}
            optionsHeld=options;
            if(SettingsOverlay!=null && SettingsOverlay.BlocksGameplay){devices.Clear();FitdUnity_SetInput(0,0,0,-1,-1,0);return;}
            if(abi==2)devices.Poll(true);
            int joy = 0;
            if (Held(KeyCode.W) || Held(KeyCode.UpArrow)) joy |= 1;
            if (Held(KeyCode.S) || Held(KeyCode.DownArrow)) joy |= 2;
            if (Held(KeyCode.A) || Held(KeyCode.LeftArrow)) joy |= 4;
            if (Held(KeyCode.D) || Held(KeyCode.RightArrow)) joy |= 8;
            if (Held(KeyCode.Q)) joy |= 16;
            if (Held(KeyCode.E)) joy |= 32;
            if (Held(KeyCode.LeftShift)) joy |= 64;
            int key = Held(KeyCode.Escape) ? 0x1b : Held(KeyCode.Return) ? 0x1c : Held(KeyCode.Tab) ? 0x0f : 0;
#if ENABLE_INPUT_SYSTEM
            var mouse = Mouse.current;
            Vector2 point = mouse == null ? new Vector2(-1, -1) : mouse.position.ReadValue();
            int buttons = mouse == null ? 0 : (Pressed(mouse.leftButton) ? 1 : 0) | (Pressed(mouse.rightButton) ? 2 : 0);
#else
            Vector2 point = Input.mousePosition;
            int buttons = (Input.GetMouseButton(0) ? 1 : 0) | (Input.GetMouseButton(1) ? 2 : 0);
#endif
            point.y = Screen.height - point.y;
            bool inside = viewport.Contains(point);
            float x = inside && Frame != null ? (point.x - viewport.x) * Frame.width / viewport.width : -1;
            float y = inside && Frame != null ? (point.y - viewport.y) * Frame.height / viewport.height : -1;
            FitdUnity_SetInput(joy, key, Held(KeyCode.Space) ? 1 : 0, x, y, inside ? buttons : 0);
        }

        private void OnGUI()
        {
            if (closing) return;
            if(SettingsOverlay!=null && (SettingsOverlay.BlocksGameplay && !attached || Frame!=null && RenderedFrames>0) && Error==null){viewport=SettingsOverlay.GameRect();return;}
            GUI.DrawTexture(new Rect(0, 0, Screen.width, Screen.height), Texture2D.blackTexture,ScaleMode.StretchToFill,false);
            if (Error != null) { GUI.Label(new Rect(20, 20, Screen.width - 40, 120), Error); return; }
            if (Frame == null || RenderedFrames==0) return;
            if(SettingsOverlay!=null){viewport=SettingsOverlay.GameRect();return;}
            viewport = SettingsOverlay!=null?SettingsOverlay.GameRect():new Rect(0,0,Screen.width,Screen.height);
            GUI.DrawTexture(viewport, SettingsOverlay!=null?SettingsOverlay.DisplayTexture(Frame):Frame, ScaleMode.StretchToFill, false);
        }

        private void OnDisable()
        {
#if ENABLE_INPUT_SYSTEM
            if(pacingConfigured)InputSystem.pollingFrequency=previousPollingFrequency;
#endif
            if(pacingConfigured){QualitySettings.vSyncCount=previousVsync;UnityEngine.Rendering.OnDemandRendering.renderFrameInterval=previousRenderInterval;Application.targetFrameRate=previousTarget;Application.runInBackground=previousBackground;pacingConfigured=false;}
            StopAllCoroutines();
            if(packetPin.IsAllocated)packetPin.Free();pixels=null;
            GetComponent<FitdAudioOutput>()?.End();
            if(abi==2)devices.Clear();
            if (owner == this)
            {
                if(attached){
                    FitdUnity_SetInput(0,0,0,-1,-1,0);
                    if(Application.isEditor)FitdUnity_SetPaused(1);
                    else {FitdUnity_Stop();if(!closing){closing=true;Application.Quit(0);}}
                }
                attached = false; owner = null;
            }
            if (abi == 1 && Frame != null) Destroy(Frame);
            gpu?.Dispose(); gpu = null; Frame = null;
        }
        private void OnApplicationPause(bool paused) { if (attached) FitdUnity_SetPaused(paused ? 1 : 0); }
        private void ShutdownHost(int exitCode)
        {
            if(closing)return;closing=true;attached=false;
            GetComponent<FitdAudioOutput>()?.End();
            if(abi==1 || abi==2)FitdUnity_Stop();
#if UNITY_EDITOR
            UnityEditor.EditorApplication.isPlaying=false;
#else
            Application.Quit(exitCode);
#endif
        }
        private void OnApplicationQuit(){if(abi!=0 && !Application.isEditor){closing=true;GetComponent<FitdAudioOutput>()?.End();FitdUnity_Stop();content?.Dispose();}}
        public void SubmitInput(int directions,int menuKey,bool action,Vector2 pointerNormalized,int buttons=0)
        {
            if(!attached)return;
            FitdUnity_SetInput(directions,menuKey,action?1:0,pointerNormalized.x*(Frame!=null?Frame.width:renderWidth),pointerNormalized.y*(Frame!=null?Frame.height:renderHeight),buttons);
        }
    }
}
