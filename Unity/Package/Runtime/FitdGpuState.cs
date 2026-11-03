using System;
using System.Collections.Generic;
using UnityEngine;

namespace Fitd.Unity
{
    // Unity objects and their IDs survive Editor domain reload; C++ can keep its
    // gameplay session paused without recreating textures or losing frozen menus.
    public sealed class FitdGpuState : ScriptableObject
    {
        public RenderTexture frame;
        public List<Image> images = new List<Image>();
        public List<Framebuffer> framebuffers = new List<Framebuffer>();
        public List<Program> programs = new List<Program>();
        public List<Uniform> uniforms = new List<Uniform>();
        [Serializable] public sealed class Image { public int id; public Texture texture; public int kind; public bool flip; }
        [Serializable] public sealed class Framebuffer { public int id; public int[] attachments; }
        [Serializable] public sealed class Program { public int id; public Shader shader; }
        [Serializable] public sealed class Uniform { public int id; public string name; public int type, count; }
    }
}
