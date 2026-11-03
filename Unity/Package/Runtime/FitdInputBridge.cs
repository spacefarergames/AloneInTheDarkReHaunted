using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using UnityEngine;
#if ENABLE_INPUT_SYSTEM
using UnityEngine.InputSystem;
#endif

namespace Fitd.Unity
{
    // SDL scancodes are USB HID positions, not operating-system key values.
    // Keep that stable binding format while Unity owns all device access.
    internal sealed class FitdInputBridge
    {
        [DllImport(FitdPlayer.Library,CallingConvention=CallingConvention.Cdecl)] private static extern void FitdUnity_SetKeyboardState([In] byte[] keys,int count);
        [DllImport(FitdPlayer.Library,CallingConvention=CallingConvention.Cdecl)] private static extern void FitdUnity_SetGamepad(int connected,uint buttons,float lx,float ly,float rx,float ry,float lt,float rt);
        private readonly byte[] keyboard = new byte[512];
        private static readonly Dictionary<KeyCode,int> scan = CreateMap();
#if ENABLE_INPUT_SYSTEM
        private static readonly Dictionary<KeyCode,Key> inputKeys = CreateInputMap();
#endif
        private static Dictionary<KeyCode,int> CreateMap()
        {
            var result=new Dictionary<KeyCode,int>();
            for(int i=0;i<26;i++)result[(KeyCode)((int)KeyCode.A+i)]=4+i;
            for(int i=0;i<9;i++)result[(KeyCode)((int)KeyCode.Alpha1+i)]=30+i;
            result[KeyCode.Alpha0]=39;
            KeyCode[] basic={KeyCode.Return,KeyCode.Escape,KeyCode.Backspace,KeyCode.Tab,KeyCode.Space,KeyCode.Minus,KeyCode.Equals,KeyCode.LeftBracket,KeyCode.RightBracket,KeyCode.Backslash};
            for(int i=0;i<basic.Length;i++)result[basic[i]]=40+i;
            result[KeyCode.Semicolon]=51;result[KeyCode.Quote]=52;result[KeyCode.BackQuote]=53;
            result[KeyCode.Comma]=54;result[KeyCode.Period]=55;result[KeyCode.Slash]=56;result[KeyCode.CapsLock]=57;
            for(int i=0;i<12;i++)result[(KeyCode)((int)KeyCode.F1+i)]=58+i;
            KeyCode[] navigation={KeyCode.Print,KeyCode.ScrollLock,KeyCode.Pause,KeyCode.Insert,KeyCode.Home,KeyCode.PageUp,KeyCode.Delete,KeyCode.End,KeyCode.PageDown,KeyCode.RightArrow,KeyCode.LeftArrow,KeyCode.DownArrow,KeyCode.UpArrow,KeyCode.Numlock,KeyCode.KeypadDivide,KeyCode.KeypadMultiply,KeyCode.KeypadMinus,KeyCode.KeypadPlus,KeyCode.KeypadEnter};
            for(int i=0;i<navigation.Length;i++)result[navigation[i]]=70+i;
            for(int i=1;i<=9;i++)result[(KeyCode)((int)KeyCode.Keypad0+i)]=88+i;
            result[KeyCode.Keypad0]=98;result[KeyCode.KeypadPeriod]=99;
            result[KeyCode.LeftControl]=224;result[KeyCode.LeftShift]=225;result[KeyCode.LeftAlt]=226;result[KeyCode.LeftCommand]=227;
            result[KeyCode.LeftWindows]=227;result[KeyCode.RightWindows]=231;
            result[KeyCode.RightControl]=228;result[KeyCode.RightShift]=229;result[KeyCode.RightAlt]=230;result[KeyCode.RightCommand]=231;
            return result;
        }
#if ENABLE_INPUT_SYSTEM
        internal static bool TryScancode(Key key,out int value){foreach(var entry in inputKeys)if(entry.Value==key){value=scan[entry.Key];return true;}value=0;return false;}
        private static Dictionary<KeyCode,Key> CreateInputMap()
        {
            var result=new Dictionary<KeyCode,Key>();
            foreach(var entry in scan)
            {
                string name=entry.Key.ToString().Replace("Alpha","Digit").Replace("Keypad","Numpad").Replace("Command","Meta").Replace("Windows","Meta");
                if(name=="Return")name="Enter";if(name=="Quote")name="Quote";if(name=="Print")name="PrintScreen";
                Key key;if(Enum.TryParse(name,true,out key))result[entry.Key]=key;
            }
            return result;
        }
#endif
        public void Poll(bool focused)
        {
            Array.Clear(keyboard,0,keyboard.Length);
            if(focused)
            {
                foreach(var entry in scan)
                {
#if ENABLE_INPUT_SYSTEM
                    Key key;bool pressed=Keyboard.current!=null && inputKeys.TryGetValue(entry.Key,out key) && (Keyboard.current[key].isPressed || Keyboard.current[key].wasPressedThisFrame);
#else
                    bool pressed=Input.GetKey(entry.Key);
#endif
                    if(pressed)keyboard[entry.Value]=1;
                }
            }
            FitdUnity_SetKeyboardState(keyboard,keyboard.Length);
#if ENABLE_INPUT_SYSTEM
            var pad=focused?Gamepad.current:null;
            if(pad!=null)
            {
                uint buttons=0;
                if(pad.buttonSouth.isPressed || pad.buttonSouth.wasPressedThisFrame)buttons|=1u<<0;if(pad.buttonEast.isPressed || pad.buttonEast.wasPressedThisFrame)buttons|=1u<<1;
                if(pad.buttonWest.isPressed || pad.buttonWest.wasPressedThisFrame)buttons|=1u<<2;if(pad.buttonNorth.isPressed || pad.buttonNorth.wasPressedThisFrame)buttons|=1u<<3;
                if(pad.selectButton.isPressed || pad.selectButton.wasPressedThisFrame)buttons|=1u<<4;if(pad.startButton.isPressed || pad.startButton.wasPressedThisFrame)buttons|=1u<<6;
                if(pad.leftStickButton.isPressed || pad.leftStickButton.wasPressedThisFrame)buttons|=1u<<7;if(pad.rightStickButton.isPressed || pad.rightStickButton.wasPressedThisFrame)buttons|=1u<<8;
                if(pad.leftShoulder.isPressed || pad.leftShoulder.wasPressedThisFrame)buttons|=1u<<9;if(pad.rightShoulder.isPressed || pad.rightShoulder.wasPressedThisFrame)buttons|=1u<<10;
                if(pad.dpad.up.isPressed || pad.dpad.up.wasPressedThisFrame)buttons|=1u<<11;if(pad.dpad.down.isPressed || pad.dpad.down.wasPressedThisFrame)buttons|=1u<<12;
                if(pad.dpad.left.isPressed || pad.dpad.left.wasPressedThisFrame)buttons|=1u<<13;if(pad.dpad.right.isPressed || pad.dpad.right.wasPressedThisFrame)buttons|=1u<<14;
                Vector2 left=pad.leftStick.ReadUnprocessedValue(),right=pad.rightStick.ReadUnprocessedValue();
                FitdUnity_SetGamepad(1,buttons,left.x,-left.y,right.x,-right.y,pad.leftTrigger.ReadUnprocessedValue(),pad.rightTrigger.ReadUnprocessedValue());return;
            }
#endif
            FitdUnity_SetGamepad(0,0,0,0,0,0,0,0);
        }
        public void Clear(){Array.Clear(keyboard,0,keyboard.Length);FitdUnity_SetKeyboardState(keyboard,keyboard.Length);FitdUnity_SetGamepad(0,0,0,0,0,0,0,0);}
    }
}
