using System;
using System.Collections.Generic;
using System.Linq;
using UnityEngine;
using UnityEngine.UIElements;
using Cursor = UnityEngine.Cursor;
#if ENABLE_INPUT_SYSTEM
using UnityEngine.InputSystem;
#endif
namespace Fitd.Unity {
 public sealed class FitdSettingsOverlay:MonoBehaviour {
  public FitdSettingsClient Client {get;private set;}
  public float Progress=>progress;
  public bool BlocksGameplay=>targetOpen || progress>.001f;
  public bool CapturingKey=>binding>=0;
  public string CurrentCategory=>category;
  FitdPlayer player;UIDocument document;PanelSettings panel;
  VisualElement root,card,previewOutline,body;
  ScrollView scroll;Label pageTitle,pageHint,toast,keyboardHint;
  Button resume,closeButton,defaultsButton,reloadButton;
  Image logo;
  readonly List<Action> refresh=new List<Action>();
  readonly Dictionary<string,Button> tabs=new Dictionary<string,Button>();
  string category="Display";float progress,velocity;bool targetOpen,closing,saveThenClose,typing;
  int hostAction=-1;
  int binding=-1;double toastUntil;Texture2D gradient;Texture2D poster;
  Image backdropImage,gameImage;
  CursorLockMode oldLock;bool oldCursor;
  static readonly string[] Categories={"Display","Lighting","Color & motion","Controls","Interface","Content","Diagnostics","Game mode","About"};
  static readonly string[] Subtitles={"Artwork, clarity and presentation","Shape the light. Keep the atmosphere.","A cinematic finish, in your style.","Find the controls that feel right.","Typography, sound and comfort.","Replacement art and creator tools.","Tools for investigating game behavior.","Choose which game to launch.","Build details and open source credits."};
  public void Initialize(FitdPlayer owner){
   if(document!=null)Destroy(document);if(panel!=null)Destroy(panel);if(gradient!=null)Destroy(gradient);
   refresh.Clear();tabs.Clear();progress=velocity=0;closing=targetOpen=false;
   player=owner;Client=new FitdSettingsClient(player);
   panel=ScriptableObject.CreateInstance<PanelSettings>();panel.name="FITD Settings Panel";panel.scaleMode=PanelScaleMode.ScaleWithScreenSize;panel.referenceResolution=new Vector2Int(1920,1080);panel.match=.5f;panel.sortingOrder=100;
   panel.themeStyleSheet=Resources.Load<ThemeStyleSheet>("FitdUI/RuntimeTheme");
   document=gameObject.AddComponent<UIDocument>();document.panelSettings=panel;document.sortingOrder=100;
   root=document.rootVisualElement;root.name="fitd-settings";root.AddToClassList("fitd-root");root.pickingMode=PickingMode.Ignore;
   root.styleSheets.Add(Resources.Load<StyleSheet>("FitdUI/Settings"));
   root.style.unityFont=Resources.GetBuiltinResource<Font>("LegacyRuntime.ttf");
   poster=Resources.Load<Texture2D>("FitdUI/Designer");
   gradient=new Texture2D(1,128,TextureFormat.RGBA32,false,true);gradient.wrapMode=TextureWrapMode.Clamp;
   for(int i=0;i<128;i++)gradient.SetPixel(0,i,Color.Lerp(new Color(.025f,.033f,.05f),new Color(.085f,.105f,.145f),i/127f));gradient.Apply();
   category="Display";
   Build();Client.Changed+=Refresh;Client.Completed+=OnCompleted;
   root.RegisterCallback<NavigationCancelEvent>(e=>{if(binding>=0)CancelBinding();else Close();e.StopPropagation();});
   if(player.AwaitingStart || FitdSettingsClient.FitdUnity_OptionsOpen()!=0)Open();
  }
  VisualElement Add(VisualElement parent,string css,string name=null){var e=new VisualElement();e.AddToClassList(css);if(name!=null)e.name=name;parent.Add(e);return e;}
  Label Label(VisualElement parent,string text,string css){var e=new Label(text);e.AddToClassList(css);parent.Add(e);return e;}
  Button Button(VisualElement parent,string text,string css,Action action){var b=new Button(action){text=text};b.AddToClassList(css);parent.Add(b);return b;}
  void Build(){
   backdropImage=new Image{image=gradient,scaleMode=ScaleMode.StretchToFill,pickingMode=PickingMode.Ignore};backdropImage.style.position=Position.Absolute;backdropImage.style.left=backdropImage.style.top=backdropImage.style.right=backdropImage.style.bottom=0;root.Add(backdropImage);
   gameImage=new Image{scaleMode=ScaleMode.StretchToFill,pickingMode=PickingMode.Ignore};gameImage.style.position=Position.Absolute;root.Add(gameImage);
   logo=new Image{image=Resources.Load<Texture2D>("FitdUI/Logo"),scaleMode=ScaleMode.StretchToFill,pickingMode=PickingMode.Ignore};
   logo.uv=new Rect(128f/1536,394f/1024,1280f/1536,390f/1024);
   logo.AddToClassList("game-logo");root.Add(logo);
   previewOutline=Add(root,"preview-outline");previewOutline.pickingMode=PickingMode.Ignore;
   keyboardHint=Label(root,"F1  SETTINGS     ESC  RETURN     TAB / ARROWS  NAVIGATE","keyboard-hint");keyboardHint.pickingMode=PickingMode.Ignore;
   card=Add(root,"settings-card");card.name="settings-card";
   var header=Add(card,"card-header");var titles=Add(header,"header-titles");Label(titles,"YOUR EXPERIENCE","eyebrow");Label(titles,"Settings","card-title");
   var close=closeButton=Button(header,"","icon-button",Close);close.name="close-settings";close.tooltip="Return to game";close.Add(new FitdSettingsIcon(7));
   var content=Add(card,"settings-content");var rail=Add(content,"category-rail");
   for(int i=0;i<Categories.Length;i++){string group=Categories[i];var tab=Button(rail,"","category",()=>SelectCategory(group));tab.name="category-"+i;tab.Add(new FitdSettingsIcon(i==7?3:i));tab.Add(new Label(group));tabs.Add(group,tab);}
   body=Add(content,"settings-body");pageTitle=Label(body,"Display","page-title");pageHint=Label(body,Subtitles[0],"page-hint");
   scroll=new ScrollView(ScrollViewMode.Vertical){name="settings-scroll"};scroll.AddToClassList("settings-scroll");body.Add(scroll);
   toast=Label(card,"Changes preview live. Save to keep them.","settings-status");toast.name="settings-status";
   var footer=Add(card,"card-footer");defaultsButton=Button(footer,"Defaults","quiet-button",()=>RequestHostAction(2));defaultsButton.name="restore-defaults";reloadButton=Button(footer,"Reload saved","quiet-button",()=>RequestHostAction(1));reloadButton.name="reload-settings";
   resume=Button(footer,"Save & return","primary-button",SaveOrStart);resume.name="save-settings";
   RenderPage();
  }
  public void SelectCategory(string value){if(!Categories.Contains(value))return;category=value;RenderPage();}
  void RenderPage(){
   if(scroll==null)return;
   refresh.Clear();scroll.Clear();
   pageTitle.text=category;pageHint.text=Subtitles[Array.IndexOf(Categories,category)];
   foreach(var pair in tabs)pair.Value.EnableInClassList("selected",pair.Key==category);
   if(category=="About"){
    AddAbout();
    return;
   }
   if(category=="Game mode"){
    AddHostSettings();
    return;
   }
   if(category=="Display"){
    AddDisplayMode();
    AddRenderScale();
   }
   foreach(var item in Client.Settings){
    if(item.group!=category || (item.id==3 || item.id==4 || item.id==51 || item.id==52))continue;
    if(!string.IsNullOrEmpty(item.section))Label(scroll,item.section.ToUpperInvariant(),"section-label");
    AddSetting(item);
   }
   if(category=="Controls")AddBindings();
   if(category=="Interface"){
    AddStartupPreference();
    AddText("Font file",0,"A bundled filename or a custom font path.");AddText("Music folder",1,"Used for external tracks when the next track loads.");
    Label(scroll,"SOUND & COMFORT","section-label");
    LocalSlider("Master volume",0,1,()=>Client.Volume,Client.SetVolume,"Affects the Unity audio output.");
    var row=Row("Reduce motion","Use a shorter, quieter settings transition.");var toggle=new Toggle();toggle.AddToClassList("switch");row.Add(toggle);toggle.SetValueWithoutNotify(Client.ReduceMotion);toggle.RegisterValueChangedCallback(e=>Client.SetMotion(e.newValue));
    LocalSlider("Interface scale",.85f,1.25f,()=>Client.UiScale,Client.SetScale,"Adjust the size of settings text and controls.");
   }
   scroll.contentContainer.style.opacity=0;scroll.contentContainer.style.translate=new Translate(0,6);scroll.schedule.Execute(()=>{scroll.contentContainer.style.opacity=1;scroll.contentContainer.style.translate=new Translate(0,0);}).ExecuteLater(10);
   Refresh();
  }
  void AddAbout()
  {
   Label(scroll,"Alone in The Dark Rehaunted","page-title");
   Label(scroll,"By Spacefarer Retro Remasters","page-hint");
   string stamp = "Editor session";
   if (!Application.isEditor)
   {
    string path = System.IO.Path.Combine(Application.streamingAssetsPath, "FitdBuildTimestamp.txt");
    stamp = System.IO.File.Exists(path) ? System.IO.File.ReadAllText(path).Trim() : "Unavailable";
   }
   Row("Version "+Application.version,"Build: "+stamp);

   var links=Add(scroll,"card-footer");
   Button(links,"GitHub source","quiet-button",
    ()=>Application.OpenURL("https://github.com/spacefarergames/AloneInTheDarkReHaunted"));
   Button(links,"Official website","quiet-button",
    ()=>Application.OpenURL("https://spacefarergames.com/aitdremaster/"));

   Label(scroll,"OPEN SOURCE CREDITS","section-label");
   Row("FITD / Free in the Dark","Original engine by yaz0r and contributors. GPL.");
   Row("SDL 3","Platform and timing utilities. zlib license.");
   Row("Dear ImGui","Native game UI and font atlas. MIT.");
   Row("bgfx, bx and bimg","Render API and image utilities. BSD 2-Clause.");
   Row("zlib","Archive decompression. zlib license.");
   Row("SoLoud","Audio mixing. zlib license.");
   Row("dr_libs and stb_vorbis","FLAC, MP3, WAV and Vorbis decoding.");
   Row("stb","Image loading, PNG writing, font rasterization and rectangle packing.");
   Row("bimg bundled codecs","Arm ASTC Encoder and miniz.");
   Row("MAME FMOPL","OPL synthesis by Tatsuyuki Satoh and Jarek Burczynski.");
   Row("UnPAK / explode","PAK decompression by Cyril Voila and Mark Adler.");
   Row("Unity Input System","Unity Companion License.");
  }
  void AddDisplayMode()
  {
   var row=Row("Display mode","Switch between a window and borderless fullscreen.");
   var choice=new DropdownField(new List<string>{"Windowed","Fullscreen"},Screen.fullScreen?1:0){name="control-52"};
   choice.AddToClassList("binding-pad");
   choice.SetEnabled(!Application.isEditor);
   if(Application.isEditor)row.tooltip="Available in the standalone player.";
   row.Add(choice);
   choice.RegisterValueChangedCallback(e=>Client.Set(52,e.newValue=="Fullscreen"?1:0));
  }
  void AddRenderScale()
  {
   var row=Row("Render scale","Changes the internal resolution used to render the game.");
   var controls=Add(row,"slider-controls");
   var value=Label(controls,"","value-label");
   var slider=new Slider(.5f,2f){name="render-scale"};
   slider.AddToClassList("setting-slider");
   controls.Add(slider);
   void UpdateValue() {
    var size=player.RenderSize;
    value.text=$"{player.RenderScale:P0}  |  {size.x} x {size.y}";
   }
   slider.SetValueWithoutNotify(player.RenderScale);
   UpdateValue();
   slider.RegisterValueChangedCallback(e=>{
    player.SetRenderScale(Mathf.Round(e.newValue*20)/20);
    UpdateValue();
   });
  }
  void AddHostSettings()
  {
   var row=Row("Game mode",player.AwaitingStart?"Select the game to start.":"The selection applies the next time you launch Unity or the player.");
   var choice=new DropdownField(new List<string>{"AITD1","JACK"},player.JackSelected?1:0){name="game-mode"};
   choice.AddToClassList("binding-pad");
   choice.SetEnabled(player.CanSelectGame);
   row.Add(choice);
   choice.RegisterValueChangedCallback(e=>player.JackSelected=e.newValue=="JACK");
   if(!player.CanSelectGame)Label(scroll,"JACK content is unavailable in this build.","page-hint");
  }
  void AddStartupPreference()
  {
   var startup=Row("Skip settings at startup","Press F1 to open settings at any time.");
   var toggle=new Toggle{value=player.SkipStartupSettings,name="skip-startup-settings"};
   toggle.AddToClassList("switch");
   startup.Add(toggle);
   toggle.RegisterValueChangedCallback(e=>player.SkipStartupSettings=e.newValue);
  }
  void SaveOrStart()
  {
   if(Client.Busy)return;
   saveThenClose=true;
   Client.Request(0);
  }
  void RequestHostAction(int action)
  {
   if(Client.Busy)return;
   hostAction=action;
   Client.Request(action);
  }
  public void EngineStarted()
  {
   player.SetAudioVolume(Client.Volume);
   player.SetTextureFiltering(Client.Values[1]>.5f);
   RenderPage();
  }
  VisualElement Row(string title,string description){var row=Add(scroll,"setting-row");var words=Add(row,"setting-words");Label(words,title,"setting-title");if(!string.IsNullOrEmpty(description))Label(words,description,"setting-description");return row;}
  void AddSetting(FitdSetting item){
   var row=Row(item.title,item.description);row.name="setting-"+item.id;row.tooltip=item.description;
   if(item.kind=="toggle" || item.kind=="fullscreen"){
    var field=new Toggle();field.AddToClassList("switch");field.name="control-"+item.id;row.Add(field);field.RegisterValueChangedCallback(e=>Client.Set(item.id,e.newValue?1:0));
    refresh.Add(()=>{field.SetValueWithoutNotify(item.id==52?Screen.fullScreen:Client.Values[item.id]>.5f);row.SetEnabled(item.id!=52 || !Application.isEditor);});
    if(item.id==52 && Application.isEditor)row.tooltip="Available in the standalone player.";
   }else{
    var controls=Add(row,"slider-controls");var value=Label(controls,"","value-label");var slider=new Slider(item.min,item.max){name="control-"+item.id};slider.AddToClassList("setting-slider");controls.Add(slider);
    slider.RegisterValueChangedCallback(e=>Client.Set(item.id,item.kind=="int"?Mathf.Round(e.newValue):e.newValue));
    refresh.Add(()=>{slider.SetValueWithoutNotify(Client.Values[item.id]);value.text=item.kind=="int" || item.max>20?Client.Values[item.id].ToString("F0"):Client.Values[item.id].ToString("F2");});
   }
   if(item.parent>=0)refresh.Add(()=>row.SetEnabled(Client.Values[item.parent]>.5f));
  }
  void LocalSlider(string title,float min,float max,Func<float> get,Action<float> set,string description){var row=Row(title,description);var controls=Add(row,"slider-controls");var value=Label(controls,"","value-label");var slider=new Slider(min,max);slider.AddToClassList("setting-slider");controls.Add(slider);slider.SetValueWithoutNotify(get());value.text=get().ToString("F2");slider.RegisterValueChangedCallback(e=>{set(e.newValue);value.text=e.newValue.ToString("F2");});}
  void AddText(string title,int id,string hint){var row=Row(title,hint);row.AddToClassList("text-row");var field=new TextField(){isDelayed=true,name="setting-text-"+id};field.AddToClassList("path-field");row.Add(field);field.RegisterCallback<FocusInEvent>(_=>typing=true);field.RegisterCallback<FocusOutEvent>(_=>typing=false);field.RegisterValueChangedCallback(e=>Client.SetText(id,e.newValue));refresh.Add(()=>{if(!typing)field.SetValueWithoutNotify(Client.Text[id]??"");});}
  static readonly string[] Actions={"Move forward","Move backward","Turn left","Turn right","Action","Confirm","Cancel","Quick turn left","Quick turn right","Run"};
  static readonly int[] PadCodes={-1,0,1,2,3,4,6,7,8,9,10,11,12,13,14};
  static readonly string[] PadNames={"Unbound","South / A","East / B","West / X","North / Y","Select","Start","Left stick","Right stick","Left shoulder","Right shoulder","D-pad up","D-pad down","D-pad left","D-pad right"};
  void AddBindings(){
   Label(scroll,"BUTTON MAPPING","section-label");Label(scroll,"Select a keyboard binding, then press a key. Escape cancels.","page-hint");
   for(int i=0;i<10;i++){int action=i;var row=Row(Actions[i],null);row.AddToClassList("binding-row");var key=Button(row,"","binding-button",()=>{binding=action;Status("Press a key for "+Actions[action]+"… Escape cancels.",true);});key.name="bind-key-"+i;
    var choices=PadNames.ToList();string current=PadName((int)Client.Values[63+i]);if(!choices.Contains(current))choices.Add(current);
    var pad=new DropdownField(choices,current){name="bind-pad-"+i};pad.AddToClassList("binding-pad");row.Add(pad);pad.RegisterValueChangedCallback(e=>{int n=Array.IndexOf(PadNames,e.newValue);if(n>=0)Client.Set(63+action,PadCodes[n]);});
    refresh.Add(()=>{key.text=binding==action?"Press a key…":KeyName((int)Client.Values[53+action]);pad.SetValueWithoutNotify(PadName((int)Client.Values[63+action]));});
   }
  }
  static string PadName(int value){int i=Array.IndexOf(PadCodes,value);return i<0?"Unbound":PadNames[i];}
  static string KeyName(int code){if(code>=4 && code<=29)return ((char)('A'+code-4)).ToString();if(code>=30 && code<=38)return (code-29).ToString();switch(code){case 39:return "0";case 40:return "Enter";case 41:return "Escape";case 42:return "Backspace";case 43:return "Tab";case 44:return "Space";case 79:return "Right";case 80:return "Left";case 81:return "Down";case 82:return "Up";case 224:return "Left Ctrl";case 225:return "Left Shift";case 228:return "Right Ctrl";case 229:return "Right Shift";}return code==0?"Unbound":"Key "+code;}
  void CancelBinding(){binding=-1;Refresh();}
  void Refresh(){if(panel==null)return;panel.scale=Client.UiScale;foreach(var action in refresh)action();}
  void OnCompleted(string text,bool success){Status(text,success);if(success && hostAction>=1)player.LoadHostSettings(hostAction==2);hostAction=-1;if(saveThenClose){saveThenClose=false;if(success){if(player.AwaitingStart)player.StartGame();else player.SaveHostSettings();Close();}}if(!BlocksGameplay)return;RenderPage();}
  void Status(string text,bool success){toast.text=text;toast.EnableInClassList("error",!success);toastUntil=Time.realtimeSinceStartupAsDouble+4;}
  public void Toggle(){if(BlocksGameplay)Close();else Open();}
  public void Open(){if(Client==null)return;closing=false;targetOpen=true;oldLock=Cursor.lockState;oldCursor=Cursor.visible;Cursor.lockState=CursorLockMode.None;Cursor.visible=true;if(player.HasEngine)FitdSettingsClient.FitdUnity_SetOptionsOpen(1);}
  public void Close(){if(Client==null || player.AwaitingStart)return;binding=-1;targetOpen=false;closing=true;}
  void Update(){
   if(Client==null)return;
   if(player.HasEngine || player.AwaitingStart)Client.Tick();
   bool nativeOpen=player.AwaitingStart || FitdSettingsClient.FitdUnity_OptionsOpen()!=0;
   bool startup=!player.HasEngine || FitdSettingsClient.FitdUnity_OptionsStartup()!=0;
   resume.text=startup?"Start Game":"Save & return";
   keyboardHint.text=startup?"TAB / ARROWS  NAVIGATE":"F1  SETTINGS     ESC  RETURN     TAB / ARROWS  NAVIGATE";
   closeButton.style.display=!player.HasEngine?DisplayStyle.None:DisplayStyle.Flex;
   defaultsButton.SetEnabled(Client.Ready && !Client.Busy);
   reloadButton.SetEnabled(Client.Ready && !Client.Busy);
   if(!closing && nativeOpen!=targetOpen){if(nativeOpen)Open();else targetOpen=false;}
#if ENABLE_INPUT_SYSTEM
   if(BlocksGameplay && Keyboard.current!=null){
    if(binding>=0){foreach(var control in Keyboard.current.allKeys)if(control.wasPressedThisFrame){if(control.keyCode==Key.Escape){CancelBinding();break;}if(control.keyCode==Key.F1 || control.keyCode==Key.Home)break;if(FitdInputBridge.TryScancode(control.keyCode,out int code)){Client.Set(53+binding,code);binding=-1;Refresh();Status("Keyboard binding updated",true);break;}}}
    else if(Keyboard.current.escapeKey.wasPressedThisFrame)Close();
   }
#endif
   float target=targetOpen?1:0;progress=Client.ReduceMotion?Mathf.MoveTowards(progress,target,Time.unscaledDeltaTime*14):Mathf.SmoothDamp(progress,target,ref velocity,targetOpen?.13f:.10f,Mathf.Infinity,Time.unscaledDeltaTime);
   if(Mathf.Abs(progress-target)<.001f)progress=target;
   if(closing && progress==0){FitdSettingsClient.FitdUnity_SetOptionsOpen(0);closing=false;Cursor.lockState=oldLock;Cursor.visible=oldCursor;}
   backdropImage.style.opacity=progress;
   gameImage.image=DisplayTexture(player.Frame);gameImage.scaleMode=gameImage.image==poster?ScaleMode.ScaleAndCrop:ScaleMode.StretchToFill;gameImage.style.left=Length.Percent(1.8f*progress);gameImage.style.top=Length.Percent(20*progress);gameImage.style.width=Length.Percent(Mathf.Lerp(100,42,progress));gameImage.style.height=Length.Percent(Mathf.Lerp(100,PreviewHeightPercent,progress));
   bool visible=progress>.001f;card.style.display=visible?DisplayStyle.Flex:DisplayStyle.None;
   foreach(var element in root.Children())if(element!=gameImage && element!=backdropImage){element.style.opacity=progress;if(element!=card)element.style.visibility=visible?Visibility.Visible:Visibility.Hidden;}
   card.style.translate=new Translate((1-progress)*30,0);card.style.scale=new Scale(new Vector3(.985f+.015f*progress,.985f+.015f*progress,1));
   const float logoAspect=1280f/390f;
   float logoWidth=Mathf.Min(42f,16f*Screen.height*logoAspect/Mathf.Max(1,Screen.width));
   logo.style.width=Length.Percent(logoWidth);
   logo.style.height=Length.Percent(logoWidth*Screen.width/(Mathf.Max(1,Screen.height)*logoAspect));
   logo.style.left=Length.Percent(1.8f+(42f-logoWidth)/2);
   previewOutline.style.height=Length.Percent(PreviewHeightPercent);
   if(Time.realtimeSinceStartupAsDouble>toastUntil)toast.text=startup?"Choose a game, then select Start Game.":"Changes preview live. Save to keep them.";
  }
  float PreviewHeightPercent=>Mathf.Min(72,42f*Screen.width/Mathf.Max(1,Screen.height)/1.6f);
  public Rect GameRect(){return new Rect(Mathf.Lerp(0,Screen.width*.018f,progress),Mathf.Lerp(0,Screen.height*.20f,progress),Mathf.Lerp(Screen.width,Screen.width*.42f,progress),Mathf.Lerp(Screen.height,Screen.height*PreviewHeightPercent/100,progress));}
  public Texture DisplayTexture(Texture live){return progress>.001f && (player.AwaitingStart || FitdSettingsClient.FitdUnity_OptionsStartup()!=0 || live==null || player.RenderedFrames==0) && poster!=null?poster:live;}
  public void DrawBackdrop(){if(progress<=.001f || gradient==null)return;Color old=GUI.color;GUI.color=new Color(1,1,1,progress);GUI.DrawTexture(new Rect(0,0,Screen.width,Screen.height),gradient,ScaleMode.StretchToFill);GUI.color=old;}
  void OnDisable(){if(document!=null)document.enabled=false;if(gradient!=null)Destroy(gradient);if(panel!=null)Destroy(panel);}
 }
 public sealed class FitdSettingsIcon:VisualElement {
  readonly int kind;
  public FitdSettingsIcon(int value){kind=value;AddToClassList("settings-icon");pickingMode=PickingMode.Ignore;generateVisualContent+=Paint;}
  void Paint(MeshGenerationContext context){var p=context.painter2D;p.strokeColor=new Color(.80f,.71f,.54f);p.lineWidth=1.7f;p.lineCap=LineCap.Round;float s=Mathf.Min(contentRect.width,contentRect.height)/24;
   Action<float[]> line=points=>{p.BeginPath();p.MoveTo(new Vector2(points[0],points[1])*s);for(int i=2;i<points.Length;i+=2)p.LineTo(new Vector2(points[i],points[i+1])*s);p.Stroke();};
   switch(kind){
    case 0:line(new float[]{3,4,21,4,21,17,3,17,3,4});line(new float[]{8,21,16,21});line(new float[]{12,17,12,21});break;
    case 1:for(int i=0;i<8;i++){float a=i*Mathf.PI/4;line(new float[]{12+Mathf.Cos(a)*8,12+Mathf.Sin(a)*8,12+Mathf.Cos(a)*10,12+Mathf.Sin(a)*10});}var circle=new List<float>();for(int i=0;i<=16;i++){circle.Add(12+Mathf.Cos(i*Mathf.PI/8)*4);circle.Add(12+Mathf.Sin(i*Mathf.PI/8)*4);}line(circle.ToArray());break;
    case 2:line(new float[]{5,3,5,21});line(new float[]{12,3,12,21});line(new float[]{19,3,19,21});line(new float[]{2,8,8,8});line(new float[]{9,16,15,16});line(new float[]{16,10,22,10});break;
    case 3:line(new float[]{6,7,18,7,22,18,18,20,14,16,10,16,6,20,2,18,6,7});line(new float[]{5,12,11,12});line(new float[]{8,9,8,15});line(new float[]{17,11,17,12});line(new float[]{19,14,19,15});break;
    case 4:line(new float[]{4,5,20,5});line(new float[]{12,5,12,21});line(new float[]{8,21,16,21});break;
    case 5:line(new float[]{3,7,10,7,12,10,21,10,21,20,3,20,3,7});line(new float[]{3,7,3,4,11,4,14,7,21,7,21,10});break;
    case 6:line(new float[]{3,5,21,5,21,20,3,20,3,5});line(new float[]{7,10,10,13,7,16});line(new float[]{13,16,17,16});break;
    case 7:line(new float[]{6,6,18,18});line(new float[]{18,6,6,18});break;
    case 8:
     var infoRing=new List<float>();
     for(int i=0;i<=24;i++){infoRing.Add(12+Mathf.Cos(i*Mathf.PI/12)*9);infoRing.Add(12+Mathf.Sin(i*Mathf.PI/12)*9);}
     line(infoRing.ToArray());line(new float[]{12,11,12,17});line(new float[]{12,7,12,8});break;
   }
  }
 }
}
