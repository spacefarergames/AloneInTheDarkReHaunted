#include "unityGpuBridge.h"
#include "common.h"
#include "configRemaster.h"
#include "controlsMenu.h"
#include "input.h"
#include "fontTTF.h"
#include "remasterOptions.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <deque>
#include <mutex>
#include <string>
#include "generatedSettings.h"
extern bool g_playWorldActive;
namespace {
struct Command{int kind,id,serial;float value;std::string text;};
std::mutex mutex;
std::deque<Command> pending;
std::array<float,FitdSettingCount> snapshot{};
std::array<std::string,2> strings;
int revision=0,serial=0,completed=0,result=0;
void syncInput(){
 g_controllerConfig.analogDeadzone=g_remasterConfig.controller.analogDeadzone;
 g_controllerConfig.analogSensitivity=g_remasterConfig.controller.analogSensitivity;
 g_controllerConfig.invertYAxis=g_remasterConfig.controller.invertYAxis;
 g_controllerConfig.analogMovement=g_remasterConfig.controller.analogMovement;
 for(int i=0;i<ACTION_COUNT;i++){setKeyboardBinding((KeyAction)i,(SDL_Scancode)g_remasterConfig.controls.keyBindings[i]);setGamepadBinding((KeyAction)i,(SDL_GamepadButton)g_remasterConfig.controls.gamepadBindings[i]);}
 detailLevel=g_remasterConfig.graphics.enableHDBackgrounds?1:0;
}
}
extern "C" int FitdUnity_GetState();
void setUnityRemasterConfigPath(const char* path);
void unitySettingsUpdate();

FITD_UNITY_API void FitdUnity_PrepareSettings(const char* path)
{
 if(FitdUnity_GetState()!=0 || !path || !*path)return;
 setUnityRemasterConfigPath(path);
 loadRemasterConfig();
 initDefaultKeyBindings();
 syncInput();
 unitySettingsUpdate();
}
FITD_UNITY_API void FitdUnity_FlushStartupSettings()
{
 if(FitdUnity_GetState()==0)unitySettingsUpdate();
}

FITD_UNITY_API int FitdUnity_SettingsCount(){return FitdSettingCount;}
FITD_UNITY_API int FitdUnity_GetSettings(float* values,int count){
 if(!values || count!=FitdSettingCount)return 0;
 std::lock_guard<std::mutex> lock(mutex);std::copy(snapshot.begin(),snapshot.end(),values);return revision;
}
FITD_UNITY_API void FitdUnity_GetSettingText(int id,char* text,int capacity){
 if(id<0 || id>1 || !text || capacity<1)return;
 std::lock_guard<std::mutex> lock(mutex);snprintf(text,size_t(capacity),"%s",strings[id].c_str());
}
FITD_UNITY_API void FitdUnity_SetSetting(int id,float value){
 if(id<0 || id>=FitdSettingCount || id==3 || id==4 || !std::isfinite(value))return;
 std::lock_guard<std::mutex> lock(mutex);
 if(pending.size()<1024)pending.push_back({0,id,0,value,{}});
}
FITD_UNITY_API void FitdUnity_SetSettingText(int id,const char* value){
 if(id<0 || id>1 || !value || std::strlen(value)>255)return;
 std::lock_guard<std::mutex> lock(mutex);if(pending.size()<1024)pending.push_back({1,id,0,0,value});
}
FITD_UNITY_API int FitdUnity_SettingsAction(int action){
 if(action<0 || action>2)return 0;
 std::lock_guard<std::mutex> lock(mutex);int request=++serial;pending.push_back({2,action,request,0,{}});return request;
}
FITD_UNITY_API int FitdUnity_SettingsResult(int* status){std::lock_guard<std::mutex> lock(mutex);if(status)*status=result;return completed;}
FITD_UNITY_API void FitdUnity_SetOptionsOpen(int open){if(open){if(!remasterOptionsIsOpen())remasterOptionsToggle();}else remasterOptionsClose();}
FITD_UNITY_API int FitdUnity_OptionsStartup(){return remasterOptionsIsStartupGate()?1:0;}
void unitySettingsUpdate(){
 int ack=0,ackResult=0;
 std::deque<Command> commands;
 {std::lock_guard<std::mutex> lock(mutex);commands.swap(pending);}
 if(!commands.empty()){
  auto before=g_remasterConfig;
  for(const auto& command:commands){
   if(command.kind==0)writeSetting(command.id,command.value);
   else if(command.kind==1){char* target=command.id==0?g_remasterConfig.font.fontPath:g_remasterConfig.music.musicFolder;snprintf(target,256,"%s",command.text.c_str());}
   else{
    bool success=true;
    if(command.id==0){syncInput();success=saveRemasterConfig();}
    if(command.id==1)loadRemasterConfig();
    if(command.id==2)initDefaultRemasterConfig();
    ack=command.serial;ackResult=success?1:-1;
   }
  }
  // Atlas updates happen at this unlocked frame boundary, never from Unity's UI thread.
  if(before.font.enableTTF!=g_remasterConfig.font.enableTTF || before.font.fontSize!=g_remasterConfig.font.fontSize || std::strcmp(before.font.fontPath,g_remasterConfig.font.fontPath)!=0)shutdownTTFFont();
  syncInput();
  if(g_playWorldActive && (before.graphics.enableHDBackgrounds!=g_remasterConfig.graphics.enableHDBackgrounds || before.masks.loadEnabled!=g_remasterConfig.masks.loadEnabled || before.masks.dumpEnabled!=g_remasterConfig.masks.dumpEnabled)){NewNumCamera=NumCamera;FlagInitView=1;}
 }
 std::array<float,FitdSettingCount> values;for(int i=0;i<FitdSettingCount;i++)values[i]=readSetting(i);
 std::array<std::string,2> text={g_remasterConfig.font.fontPath,g_remasterConfig.music.musicFolder};
 std::lock_guard<std::mutex> lock(mutex);
 if(ack){completed=ack;result=ackResult;}
 if(revision==0 || values!=snapshot || text!=strings){snapshot=values;strings=text;++revision;}
}
