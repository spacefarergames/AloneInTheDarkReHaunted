using System;
using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Threading;
using UnityEngine;

namespace Fitd.Unity
{
    [RequireComponent(typeof(AudioSource))]
    public sealed class FitdAudioOutput : MonoBehaviour
    {
        [DllImport(FitdPlayer.Library,CallingConvention=CallingConvention.Cdecl)] private static extern void FitdUnity_MixAudio(IntPtr data,uint frames);
        private readonly object gate=new object();
        private readonly float[] stereo=new float[8192];
        private GCHandle stereoPin;
        private AudioSource output;
        private bool running;
        private long callbacks,nonSilent,maxTicks;
        public long CallbackCount=>Interlocked.Read(ref callbacks);
        public long NonSilentCallbackCount=>Interlocked.Read(ref nonSilent);
        public double MaxCallbackMilliseconds=>Interlocked.Read(ref maxTicks)*1000.0/Stopwatch.Frequency;
        public double BufferedMilliseconds {get;private set;}

        internal static void ConfigureLowLatency()
        {
            var configuration=AudioSettings.GetConfiguration();
            if(configuration.dspBufferSize>256){configuration.dspBufferSize=256;AudioSettings.Reset(configuration);}
        }
        public void Begin(int sampleRate)
        {
            End();output=GetComponent<AudioSource>();
            output.playOnAwake=false;output.loop=true;output.spatialBlend=0;output.priority=0;
            output.clip=null;
            AudioSettings.GetDSPBufferSize(out int frames,out int buffers);
            BufferedMilliseconds=1000.0*frames*buffers/sampleRate;
            lock(gate){stereoPin=GCHandle.Alloc(stereo,GCHandleType.Pinned);callbacks=nonSilent=maxTicks=0;running=true;}
            // A procedural source is mixed at DSP time, without a streaming clip's read-ahead.
            output.Play();
            UnityEngine.Debug.Log("FITD_AUDIO_CONFIG: rate="+sampleRate+" frames="+frames+" buffers="+buffers+" bufferedMs="+BufferedMilliseconds.ToString("F2"));
        }
        private void OnAudioFilterRead(float[] data,int channels)
        {
            long started=Stopwatch.GetTimestamp();
            lock(gate)
            {
                if(!running || channels<=0){Array.Clear(data,0,data.Length);return;}
                int frames=data.Length/channels;
                if(channels==2 && frames<=65536)
                {
                    var pin=GCHandle.Alloc(data,GCHandleType.Pinned);
                    try{FitdUnity_MixAudio(pin.AddrOfPinnedObject(),(uint)frames);}finally{pin.Free();}
                }
                else
                {
                    int offset=0;
                    while(offset<frames){int count=Math.Min(4096,frames-offset);FitdUnity_MixAudio(stereoPin.AddrOfPinnedObject(),(uint)count);
                        for(int i=0;i<count;i++)for(int channel=0;channel<channels;channel++)data[(offset+i)*channels+channel]=channels==1?(stereo[i*2]+stereo[i*2+1])*0.5f:channel<2?stereo[i*2+channel]:0;
                        offset+=count;
                    }
                }
                Interlocked.Increment(ref callbacks);
                for(int i=0;i<data.Length;i++)if(Math.Abs(data[i])>0.00001f){Interlocked.Increment(ref nonSilent);break;}
                long ticks=Stopwatch.GetTimestamp()-started;if(ticks>maxTicks)Interlocked.Exchange(ref maxTicks,ticks);
            }
        }
        public void End()
        {
            lock(gate){running=false;if(stereoPin.IsAllocated)stereoPin.Free();}
            if(output!=null){output.Stop();output.clip=null;}
        }
        private void OnDisable(){End();}
    }
}
