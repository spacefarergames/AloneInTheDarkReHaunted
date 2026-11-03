using System;
using System.Collections.Generic;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;
using UnityEngine;
using UnityEngine.Rendering;
using Object = UnityEngine.Object;

namespace Fitd.Unity
{
    // Replays CPU gameplay draw commands using Unity meshes, ShaderLab materials,
    // render targets, depth/stencil, and command buffers. No native GPU handles.
    public sealed class FitdGpuRenderer : IDisposable
    {
        private sealed class Image { public Texture texture; public byte[] pixels; public int kind; public bool flip; }
        private sealed class Uniform { public string name; public byte type; public ushort count; }
        private sealed class DrawSlot { public Mesh mesh; public Material material; public int vertices,indices; public int[] sequential; public bool stateReady; public ulong state; public uint stencil,backStencil;
            public int submeshIndices=-1,submeshVertices=-1,topology=-1; }
        private bool filtering=true;
        public void SetTextureFiltering(bool enabled){filtering=enabled;if(Frame!=null)Frame.filterMode=enabled?FilterMode.Bilinear:FilterMode.Point;foreach(var image in images.Values)if(image.kind<=1 && image.texture is Texture2D)image.texture.filterMode=enabled?FilterMode.Bilinear:FilterMode.Point;}
        private byte[] packetBytes;
        private static readonly Bounds MeshBounds=new Bounds(Vector3.zero,Vector3.one*100000000);
        private const MeshUpdateFlags UploadFlags=MeshUpdateFlags.DontRecalculateBounds | MeshUpdateFlags.DontValidateIndices | MeshUpdateFlags.DontNotifyMeshUsers;
        private static readonly VertexAttributeDescriptor[] VertexFormat={
            new VertexAttributeDescriptor(VertexAttribute.Position,VertexAttributeFormat.Float32,4),
            new VertexAttributeDescriptor(VertexAttribute.Color,VertexAttributeFormat.Float32,4),
            new VertexAttributeDescriptor(VertexAttribute.TexCoord0,VertexAttributeFormat.Float32,4),
            new VertexAttributeDescriptor(VertexAttribute.TexCoord1,VertexAttributeFormat.Float32,4),
            new VertexAttributeDescriptor(VertexAttribute.TexCoord2,VertexAttributeFormat.Float32,4)};
        private readonly Dictionary<ushort, Image> images = new Dictionary<ushort, Image>();
        private readonly Dictionary<ushort, ushort[]> framebuffers = new Dictionary<ushort, ushort[]>();
        private readonly Dictionary<ushort, Shader> programs = new Dictionary<ushort, Shader>();
        private readonly Dictionary<ushort, Uniform> uniforms = new Dictionary<ushort, Uniform>();
        private readonly List<DrawSlot> slots = new List<DrawSlot>();
        private readonly CommandBuffer commands = new CommandBuffer { name = "FITD Unity rendering" };
        private readonly List<Object> retired = new List<Object>();
        private readonly FitdGpuState session;
        private Matrix4x4 viewProjection;
        private Vector4 viewRect;
        private int slotIndex, targetHeight;
        public RenderTexture Frame { get; private set; }
        public uint FrameNumber { get; private set; }
        public double LastFrameMilliseconds { get; private set; }
        public double TotalFrameMilliseconds { get; private set; }
        public int LastDrawCalls => slotIndex;

        public FitdGpuRenderer(int width, int height)
        {
            var sessions = Resources.FindObjectsOfTypeAll<FitdGpuState>();
            if (sessions.Length > 1) throw new InvalidOperationException("More than one FITD GPU session exists.");
            session = sessions.Length == 1 ? sessions[0] : ScriptableObject.CreateInstance<FitdGpuState>();
            session.hideFlags = HideFlags.HideAndDontSave;
            Debug.Log("FITD_GPU_SESSION: frame="+(session.frame!=null)+" images="+session.images.Count+" programs="+session.programs.Count);
            foreach(var item in session.images) {
                if(item.texture==null) throw new InvalidOperationException("Unity lost FITD texture "+item.id+" across Play mode");
                images.Add((ushort)item.id,new Image {texture=item.texture,kind=item.kind,flip=item.flip});
            }
            foreach(var item in session.programs) programs.Add((ushort)item.id,item.shader);
            foreach(var item in session.uniforms) uniforms.Add((ushort)item.id,new Uniform {name=item.name,type=(byte)item.type,count=(ushort)item.count});
            foreach(var item in session.framebuffers)
            {
                var attachments = new ushort[item.attachments.Length];
                for(int i=0;i<attachments.Length;i++)attachments[i]=(ushort)item.attachments[i];
                framebuffers.Add((ushort)item.id,attachments);
            }
            if(session.frame!=null) {Frame=session.frame;return;}
            Frame = new RenderTexture(width, height, 24, RenderTextureFormat.ARGB32, RenderTextureReadWrite.Linear) {
                name = "FITD Unity frame", hideFlags = HideFlags.HideAndDontSave, filterMode = FilterMode.Bilinear, wrapMode = TextureWrapMode.Clamp
            };
            Frame.Create();
            session.frame = Frame;
        }

        public void Render(byte[] packet, int length)
        {
            if (packet == null || length < 8 || length > packet.Length) throw new InvalidDataException("Invalid FITD render packet.");
            packetBytes=packet;
            long renderStarted=System.Diagnostics.Stopwatch.GetTimestamp();
            commands.Clear(); slotIndex = 0;
            using (var stream = new MemoryStream(packet, 0, length, false))
            using (var reader = new BinaryReader(stream, Encoding.UTF8))
            {
                if (reader.ReadUInt32() != 0x32475546) throw new InvalidDataException("FITD render packet magic mismatch.");
                FrameNumber = reader.ReadUInt32();
                while (stream.Position < stream.Length)
                {
                    ushort op = reader.ReadUInt16(); uint size = reader.ReadUInt32();
                    long end = checked(stream.Position + size);
                    if (end > stream.Length) throw new InvalidDataException("Truncated FITD render command.");
                    switch (op)
                    {
                        case 1: CreateTexture(reader); break;
                        case 2: UpdateTexture(reader); break;
                        case 3: CreateProgram(reader); break;
                        case 4: CreateUniform(reader); break;
                        case 5: CreateFramebuffer(reader); break;
                        case 6: SetView(reader); break;
                        case 7: Draw(reader); break;
                        case 8: Blit(reader); break;
                        case 9: RetireTexture(reader.ReadUInt16()); break;
                        case 10: { ushort handle=reader.ReadUInt16();framebuffers.Remove(handle);session.framebuffers.RemoveAll(item=>item.id==handle);break; }
                        case 11: ReadTexture(reader); break;
                        case 12: ResizeFrame(reader.ReadUInt16(), reader.ReadUInt16()); break;
                        default: throw new InvalidDataException("Unknown FITD render command: " + op);
                    }
                    if (stream.Position != end) throw new InvalidDataException("FITD render command size mismatch: " + op);
                }
            }
            Execute();
            foreach (var asset in retired) Object.Destroy(asset);
            retired.Clear();
            LastFrameMilliseconds=(System.Diagnostics.Stopwatch.GetTimestamp()-renderStarted)*1000.0/System.Diagnostics.Stopwatch.Frequency;TotalFrameMilliseconds+=LastFrameMilliseconds;
        }

        private void ResizeFrame(int width, int height)
        {
            if (width < 320 || height < 200 || width > 4096 || height > 4096)
                throw new InvalidDataException("Invalid FITD render size.");
            if (Frame.width == width && Frame.height == height) return;
            Frame.Release();
            Frame.width = width;
            Frame.height = height;
            Frame.Create();
        }

        private static string ReadString(BinaryReader reader)
        {
            int size = reader.ReadUInt16(); byte[] bytes = reader.ReadBytes(size);
            if (bytes.Length != size) throw new EndOfStreamException();
            return Encoding.UTF8.GetString(bytes);
        }

        private void CreateTexture(BinaryReader reader)
        {
            ushort handle = reader.ReadUInt16(); int width = reader.ReadUInt16(), height = reader.ReadUInt16(), kind = reader.ReadUInt16();
            ulong flags = reader.ReadUInt64(); int size = checked((int)reader.ReadUInt32()); byte[] pixels = reader.ReadBytes(size);
            if (pixels.Length != size || width == 0 || height == 0 || width > 8192 || height > 8192 || kind > 4) throw new InvalidDataException("Invalid native texture.");
            bool renderTarget = (flags & 0x1000000000UL) != 0 || kind == 4;
            Texture texture;
            if (renderTarget)
            {
                var format = kind == 4 ? RenderTextureFormat.Depth : kind == 2 || kind == 3 ? RenderTextureFormat.R8 : RenderTextureFormat.ARGB32;
                var render = new RenderTexture(width, height, kind == 4 ? 24 : 0, format, RenderTextureReadWrite.Linear);
                render.Create(); texture = render;
            }
            else
            {
                var format = kind == 1 ? TextureFormat.BGRA32 : kind == 2 || kind == 3 ? TextureFormat.R8 : TextureFormat.RGBA32;
                var image = new Texture2D(width, height, format, false, true);
                int expected = checked(width * height * (kind == 2 || kind == 3 ? 1 : 4));
                if (size == 0) pixels = new byte[expected];
                else if (size != expected) throw new InvalidDataException("Texture byte count mismatch.");
                image.LoadRawTextureData(pixels); image.Apply(false, false); texture = image;
            }
            texture.name = "FITD texture " + handle; texture.hideFlags = HideFlags.HideAndDontSave;
            texture.filterMode = (flags & 0x140UL) != 0 || kind == 3 || (!filtering && kind<=1 && !renderTarget) ? FilterMode.Point : FilterMode.Bilinear;
            texture.wrapMode = (flags & 10UL) != 0 ? TextureWrapMode.Clamp : TextureWrapMode.Repeat;
            if (images.ContainsKey(handle)) RetireTexture(handle);
            images.Add(handle, new Image { texture = texture, pixels = pixels, kind = kind, flip = renderTarget });
            session.images.Add(new FitdGpuState.Image {id=handle,texture=texture,kind=kind,flip=renderTarget});
        }

        private void UpdateTexture(BinaryReader reader)
        {
            var image = images[reader.ReadUInt16()]; int x = reader.ReadUInt16(), y = reader.ReadUInt16(), width = reader.ReadUInt16(), height = reader.ReadUInt16(), pitch = reader.ReadUInt16();
            int size = checked((int)reader.ReadUInt32()); byte[] data = reader.ReadBytes(size);
            var texture = image.texture as Texture2D;
            if (texture == null || data.Length != size) throw new InvalidDataException("Invalid texture update.");
            if(image.pixels==null)image.pixels=texture.GetRawTextureData<byte>().ToArray();
            int stride = image.kind == 2 || image.kind == 3 ? 1 : 4;
            if (pitch == ushort.MaxValue) pitch = width * stride;
            if (x + width > texture.width || y + height > texture.height || pitch < width * stride || (long)pitch * (height - 1) + width * stride > data.Length)
                throw new InvalidDataException("Texture update is out of bounds.");
            for (int row = 0; row < height; row++) Buffer.BlockCopy(data, row * pitch, image.pixels, ((y + row) * texture.width + x) * stride, width * stride);
            texture.LoadRawTextureData(image.pixels); texture.Apply(false, false);
        }

        private void CreateProgram(BinaryReader reader)
        {
            ushort handle = reader.ReadUInt16(); string vertex = ReadString(reader), fragment = ReadString(reader);
            var shader = Resources.Load<Shader>("FitdShaders/" + vertex + "__" + fragment);
            if (shader == null || !shader.isSupported) throw new NotSupportedException("Missing/unsupported Unity shader: " + vertex + " / " + fragment);
            programs[handle] = shader;
            session.programs.RemoveAll(item=>item.id==handle);session.programs.Add(new FitdGpuState.Program {id=handle,shader=shader});
        }

        private void CreateUniform(BinaryReader reader)
        {
            ushort handle = reader.ReadUInt16(); string name = ReadString(reader); byte type = reader.ReadByte(); ushort count = reader.ReadUInt16();
            uniforms[handle] = new Uniform { name = name, type = type, count = count };
            session.uniforms.RemoveAll(item=>item.id==handle);session.uniforms.Add(new FitdGpuState.Uniform {id=handle,name=name,type=type,count=count});
        }

        private void CreateFramebuffer(BinaryReader reader)
        {
            ushort handle = reader.ReadUInt16(); int count = reader.ReadByte(); var attachments = new ushort[count];
            for (int i = 0; i < count; i++) attachments[i] = reader.ReadUInt16(); framebuffers[handle] = attachments;
            var saved=new int[count];for(int i=0;i<count;i++)saved[i]=attachments[i];
            session.framebuffers.RemoveAll(item=>item.id==handle);session.framebuffers.Add(new FitdGpuState.Framebuffer{id=handle,attachments=saved});
        }

        private static Matrix4x4 ReadMatrix(BinaryReader reader)
        {
            var result = new Matrix4x4(); for (int i = 0; i < 16; i++) result[i] = reader.ReadSingle(); return result;
        }

        private void SetView(BinaryReader reader)
        {
            reader.ReadUInt16(); ushort framebuffer = reader.ReadUInt16();
            int x = reader.ReadUInt16(), y = reader.ReadUInt16(), width = reader.ReadUInt16(), height = reader.ReadUInt16();
            int flags = reader.ReadUInt16(); uint rgba = reader.ReadUInt32(); float depth = reader.ReadSingle(); byte stencil = reader.ReadByte();
            Matrix4x4 view = ReadMatrix(reader), projection = ReadMatrix(reader); viewProjection = projection * view;
            RenderTexture color = Frame, z = Frame;
            if (framebuffer != ushort.MaxValue)
            {
                color = null; z = null;
                foreach (ushort attachment in framebuffers[framebuffer])
                {
                    var image = images[attachment];
                    if (image.kind == 4) z = image.texture as RenderTexture;
                    else color = image.texture as RenderTexture;
                }
                if (color == null) throw new InvalidDataException("Framebuffer has no color attachment.");
                if (z == null) z = color;
            }
            commands.SetRenderTarget(new RenderTargetIdentifier(color), new RenderTargetIdentifier(z));
            targetHeight = color.height;
            commands.SetViewport(new Rect(x, targetHeight - y - height, width, height)); commands.DisableScissorRect();
            viewRect = new Vector4(x,y,width,height);
            RTClearFlags clear = RTClearFlags.None;
            if ((flags & 1) != 0) clear |= RTClearFlags.Color;
            if ((flags & 2) != 0) clear |= RTClearFlags.Depth;
            if ((flags & 4) != 0) clear |= RTClearFlags.Stencil;
            // ClearRenderTarget accepts logical depth and handles reversed Z itself.
            // Vertex output and sampled depth still need their explicit conversions.
            if (clear != RTClearFlags.None) commands.ClearRenderTarget(clear, new Color((rgba >> 24) / 255f, ((rgba >> 16) & 255) / 255f, ((rgba >> 8) & 255) / 255f, (rgba & 255) / 255f), depth, stencil);
        }

        private void Draw(BinaryReader reader)
        {
            var shader = programs[reader.ReadUInt16()];var activeUniforms=FitdShaderUniforms.Get(shader.name); ulong state = reader.ReadUInt64(); uint stencil = reader.ReadUInt32(), backStencil = reader.ReadUInt32();
            int sx = reader.ReadUInt16(), sy = reader.ReadUInt16(), sw = reader.ReadUInt16(), sh = reader.ReadUInt16();
            int count=checked((int)reader.ReadUInt32());
            if(count<=0 || count>1000000)throw new InvalidDataException("Mesh vertex count exceeds the bridge limit.");
            int vertexOffset=checked((int)reader.BaseStream.Position),vertexBytes=checked(count*80);
            if(vertexBytes>reader.BaseStream.Length-reader.BaseStream.Position)throw new InvalidDataException("Truncated mesh vertices.");
            reader.BaseStream.Position+=vertexBytes;
            int indexCount=checked((int)reader.ReadUInt32()),indexOffset=checked((int)reader.BaseStream.Position);
            if(indexCount<0 || indexCount>3000000 || (long)indexCount*4>reader.BaseStream.Length-reader.BaseStream.Position)throw new InvalidDataException("Invalid mesh indices.");
            reader.BaseStream.Position+=(long)indexCount*4;
            DrawSlot slot;
            if(slotIndex==slots.Count){slot=new DrawSlot{mesh=new Mesh{name="FITD draw "+shader.name},material=new Material(shader)};slot.mesh.MarkDynamic();slots.Add(slot);}
            else slot=slots[slotIndex];
            slotIndex++;
            if(slot.material.shader!=shader){slot.material.shader=shader;slot.mesh.name="FITD draw "+shader.name;slot.stateReady=false;}
            var mesh=slot.mesh;var material=slot.material;
            if(count>slot.vertices){slot.vertices=Mathf.NextPowerOfTwo(count);mesh.SetVertexBufferParams(slot.vertices,VertexFormat);mesh.bounds=MeshBounds;slot.submeshVertices=-1;}
            mesh.SetVertexBufferData(packetBytes,vertexOffset,0,vertexBytes,0,UploadFlags);
            int topology=(int)((state>>48)&7),used=indexCount==0?count:indexCount;
            int[] converted=null;
            if(topology==1){converted=new int[Math.Max(0,used-2)*3];for(int i=0;i<used-2;i++){int first=i+(i%2),second=i+1-(i%2);converted[i*3]=indexCount==0?first:BitConverter.ToInt32(packetBytes,indexOffset+first*4);converted[i*3+1]=indexCount==0?second:BitConverter.ToInt32(packetBytes,indexOffset+second*4);converted[i*3+2]=indexCount==0?i+2:BitConverter.ToInt32(packetBytes,indexOffset+(i+2)*4);}used=converted.Length;}
            if(used>slot.indices){slot.indices=Mathf.NextPowerOfTwo(used);mesh.SetIndexBufferParams(slot.indices,IndexFormat.UInt32);slot.submeshIndices=-1;slot.sequential=new int[slot.indices];for(int i=0;i<slot.indices;i++)slot.sequential[i]=i;}
            if(converted!=null)mesh.SetIndexBufferData(converted,0,0,used,UploadFlags);
            else if(indexCount!=0)mesh.SetIndexBufferData(packetBytes,indexOffset,0,indexCount*4,UploadFlags);
            else mesh.SetIndexBufferData(slot.sequential,0,0,used,UploadFlags);
            if(slot.submeshIndices!=used || slot.submeshVertices!=count || slot.topology!=topology){
                mesh.subMeshCount=1;
                mesh.SetSubMesh(0,new SubMeshDescriptor(0,used,topology==2?MeshTopology.Lines:topology==3?MeshTopology.LineStrip:topology==4?MeshTopology.Points:MeshTopology.Triangles){bounds=MeshBounds,vertexCount=count},UploadFlags);
                slot.submeshIndices=used;slot.submeshVertices=count;slot.topology=topology;
            }
            if(!slot.stateReady || slot.state!=state || slot.stencil!=stencil || slot.backStencil!=backStencil){
                ConfigureState(material,state,stencil,backStencil);slot.state=state;slot.stencil=stencil;slot.backStencil=backStencil;slot.stateReady=true;
            }
            material.SetMatrix("_FitdViewProj", viewProjection);
            material.SetVector("u_viewRect", viewRect);
            int uniformCount = reader.ReadUInt16();
            for (int i = 0; i < uniformCount; i++)
            {
                var uniform = uniforms[reader.ReadUInt16()]; int bytes = checked((int)reader.ReadUInt32());
                if (bytes % 4 != 0 || bytes > 65536) throw new InvalidDataException("Invalid uniform size.");
                if(activeUniforms!=null && !activeUniforms.Contains(uniform.name)){reader.BaseStream.Position+=bytes;continue;}
                var values = new float[bytes / 4]; for (int n = 0; n < values.Length; n++) values[n] = reader.ReadSingle();
                if (uniform.type == 4)
                {
                    var matrix = new Matrix4x4(); for (int n = 0; n < Math.Min(16, values.Length); n++) matrix[n] = values[n]; material.SetMatrix(uniform.name, matrix);
                }
                else
                {
                    var vectors = new Vector4[(values.Length + 3) / 4];
                    for (int n = 0; n < values.Length; n++) vectors[n/4][n%4] = values[n];
                    if (vectors.Length == 1) material.SetVector(uniform.name, vectors[0]); else material.SetVectorArray(uniform.name, vectors);
                }
            }
            int textureCount = reader.ReadByte();
            for (int i = 0; i < textureCount; i++)
            {
                ushort sampler = reader.ReadUInt16(), handle = reader.ReadUInt16(); uint flags = reader.ReadUInt32();
                Image image; Uniform uniform;
                if (handle == ushort.MaxValue || sampler == ushort.MaxValue || !images.TryGetValue(handle, out image) || !uniforms.TryGetValue(sampler, out uniform)) continue;
                if(activeUniforms!=null && !activeUniforms.Contains(uniform.name))continue;
                material.SetTexture(uniform.name, image.texture);
                material.SetFloat(uniform.name+"_FitdFlipY",image.flip?1:0);
                material.SetFloat(uniform.name+"_FitdDepthReverse",image.kind==4 && SystemInfo.usesReversedZBuffer?1:0);
                material.SetVector(uniform.name + "_TexelSize", new Vector4(1f/image.texture.width, 1f/image.texture.height, image.texture.width, image.texture.height));
            }
            if (sw == ushort.MaxValue) commands.DisableScissorRect(); else commands.EnableScissorRect(new Rect(sx, targetHeight - sy - sh, sw, sh));
            commands.DrawMesh(mesh, Matrix4x4.identity, material, 0, 0);
        }

        private static int Compare(int code, bool reverse)
        {
            int[] values = {8,2,4,3,7,5,6,1,8};
            int compare = code < values.Length ? values[code] : 8;
            if (reverse) { if (compare == 2) compare = 5; else if (compare == 5) compare = 2; else if (compare == 4) compare = 7; else if (compare == 7) compare = 4; }
            return compare;
        }

        private static void ConfigureState(Material material, ulong state, uint stencil, uint backStencil)
        {
            int[] blend = {1,0,1,3,6,5,10,7,8,2,4,9,1,0};
            int source = (int)((state >> 12) & 15), destination = (int)((state >> 16) & 15);
            int alphaSource = (int)((state >> 20) & 15), alphaDestination = (int)((state >> 24) & 15);
            if (source >= blend.Length || destination >= blend.Length || alphaSource >= blend.Length || alphaDestination >= blend.Length) throw new InvalidDataException("Unsupported blend mode.");
            material.SetInt("_SrcBlend", source == 0 ? 1 : blend[source]); material.SetInt("_DstBlend", source == 0 ? 0 : blend[destination]);
            material.SetInt("_SrcBlendAlpha", alphaSource == 0 ? (source == 0 ? 1 : blend[source]) : blend[alphaSource]);
            material.SetInt("_DstBlendAlpha", alphaSource == 0 ? (source == 0 ? 0 : blend[destination]) : blend[alphaDestination]);
            material.SetInt("_BlendOp", (int)((state >> 28) & 7)); material.SetInt("_BlendOpAlpha", (int)((state >> 31) & 7));
            // ShaderLab also converts logical comparison functions for reversed Z.
            material.SetInt("_ZTest", Compare((int)((state >> 4) & 15), false));
            material.SetInt("_ZWrite", (state & 0x4000000000UL) != 0 ? 1 : 0);
            int cull=(state & 0x1000000000UL)!=0?1:(state & 0x2000000000UL)!=0?2:0;
            // Unity handles render-target winding. Only the native front-face flag reverses it.
            if((state & 0x8000000000UL)!=0 && cull!=0)cull=3-cull;
            material.SetInt("_Cull",cull);
            int mask = ((state & 1) != 0 ? 8 : 0) | ((state & 2) != 0 ? 4 : 0) | ((state & 4) != 0 ? 2 : 0) | ((state & 8) != 0 ? 1 : 0);
            material.SetInt("_ColorMask", mask);
            int[] operations = {1,0,2,6,3,7,4,5};
            material.SetInt("_StencilRef", (int)(stencil & 255)); material.SetInt("_StencilMask", stencil == 0 ? 255 : (int)((stencil >> 8) & 255));
            material.SetInt("_StencilComp", stencil == 0 ? 8 : Compare((int)((stencil >> 16) & 15), false));
            material.SetInt("_StencilFail", stencil == 0 ? 0 : operations[(stencil >> 20) & 7]);
            material.SetInt("_StencilZFail", stencil == 0 ? 0 : operations[(stencil >> 24) & 7]);
            material.SetInt("_StencilPass", stencil == 0 ? 0 : operations[(stencil >> 28) & 7]);
            if (backStencil != 0 && backStencil != stencil) throw new NotSupportedException("Distinct front/back stencil state is not yet supported.");
        }

        private void Blit(BinaryReader reader)
        {
            ushort destinationHandle=reader.ReadUInt16(),sourceHandle=reader.ReadUInt16();
            var destination = images[destinationHandle].texture; var source = images[sourceHandle].texture;
            images[destinationHandle].flip=images[sourceHandle].flip;
            foreach(var item in session.images)if(item.id==destinationHandle)item.flip=images[sourceHandle].flip;
            int dx = reader.ReadUInt16(), dy = reader.ReadUInt16(), sx = reader.ReadUInt16(), sy = reader.ReadUInt16(), width = reader.ReadUInt16(), height = reader.ReadUInt16();
            if (width == ushort.MaxValue) width = Math.Min(source.width - sx, destination.width - dx);
            if (height == ushort.MaxValue) height = Math.Min(source.height - sy, destination.height - dy);
            commands.CopyTexture(source,0,0,sx,sy,width,height,destination,0,0,dx,dy);
        }

        private void ReadTexture(BinaryReader reader)
        {
            var texture = images[reader.ReadUInt16()].texture; ulong address = reader.ReadUInt64();
            Execute();
            var target = texture as RenderTexture;
            bool temporary = target == null;
            if (temporary) { target = RenderTexture.GetTemporary(texture.width,texture.height,0,RenderTextureFormat.ARGB32,RenderTextureReadWrite.Linear); Graphics.Blit(texture,target); }
            var previous = RenderTexture.active; RenderTexture.active = target;
            var copy = new Texture2D(texture.width,texture.height,TextureFormat.RGBA32,false,true);
            copy.ReadPixels(new Rect(0,0,texture.width,texture.height),0,0); copy.Apply();
            var bytes = copy.GetRawTextureData<byte>().ToArray(); var topDown = new byte[bytes.Length]; int stride = texture.width*4;
            for(int row=0;row<texture.height;row++) Buffer.BlockCopy(bytes,row*stride,topDown,(texture.height-1-row)*stride,stride);
            Marshal.Copy(topDown,0,new IntPtr(unchecked((long)address)),topDown.Length);
            RenderTexture.active = previous; Object.Destroy(copy); if(temporary)RenderTexture.ReleaseTemporary(target);
        }

        private void Execute() { Graphics.ExecuteCommandBuffer(commands); commands.Clear(); }
        private void RetireTexture(ushort handle) { Image image;if(images.TryGetValue(handle,out image)){retired.Add(image.texture);images.Remove(handle);session.images.RemoveAll(item=>item.id==handle);} }
        public void Dispose()
        {
            // The session's Unity objects are intentionally retained across Play-mode
            // exit and domain reload; only the command encoder and mesh pool detach.
            commands.Release(); images.Clear();
            foreach(var slot in slots){Object.Destroy(slot.mesh);Object.Destroy(slot.material);}slots.Clear();
            foreach(var asset in retired)Object.Destroy(asset);retired.Clear();
            Frame=null;
        }
    }
}
