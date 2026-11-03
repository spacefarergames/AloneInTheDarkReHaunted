using System;
using UnityEditor;
using UnityEngine;

public static class FitdShadersSmoke
{
    public static void Run()
    {
        int count = 0;
        foreach (var shader in Resources.LoadAll<Shader>("FitdShaders"))
        {
            var material = new Material(shader);
            bool supported = material.SetPass(0);
            UnityEngine.Object.DestroyImmediate(material);
            foreach (var message in ShaderUtil.GetShaderMessages(shader))
                if (message.severity.ToString() == "Error") throw new Exception(shader.name + ": " + message.message + " line " + message.line);
            if (!supported) throw new Exception("Unsupported Unity shader " + shader.name);
            count++;
        }
        if (count < 24) throw new Exception("Missing generated Unity shader pairs: " + count);
        Debug.Log("FITD_SHADERS_PASS: " + count + " Unity shader pairs compile and bind");
    }
}
