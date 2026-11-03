using System;
using System.Collections;
using System.Collections.Generic;
using UnityEditor;
using UnityEngine;

public static class FitdBundlesSmoke
{
    private static readonly Stack<IEnumerator> routines = new Stack<IEnumerator>();
    private static UnityEngine.AsyncOperation pending;
    private static Fitd.Unity.FitdBundleStore store;
    private static double deadline;

    public static void Run()
    {
        deadline = EditorApplication.timeSinceStartup + 60;
        routines.Push(Check()); EditorApplication.update += Tick;
    }

    private static IEnumerator Check()
    {
        store = new Fitd.Unity.FitdBundleStore();
        yield return store.Initialize();
        if (!store.Ready || store.Catalog.entries.Length != 5) throw new Exception("Unexpected catalog fixture.");
        if (!store.Contains("BACKGROUNDS_HD/fixture.png")) throw new Exception("Case-insensitive content lookup failed.");
        bool rejected = false;
        try { Fitd.Unity.FitdBundleStore.NormalizeKey("../escape"); } catch (ArgumentException) { rejected = true; }
        if (!rejected) throw new Exception("Traversal key was accepted.");
        TextAsset bytes = null;
        yield return store.Load<TextAsset>("test.itd", value => bytes = value);
        if (bytes == null || System.Text.Encoding.ASCII.GetString(bytes.bytes) != "FITD bundle binary round-trip\0\x01\x02") throw new Exception("Binary bundle round trip failed.");
        Texture2D texture = null;
        yield return store.Load<Texture2D>("backgrounds_hd/fixture.png", value => texture = value);
        if (texture == null || texture.width != 16 || texture.height != 16) throw new Exception("Native texture import failed.");
        AudioClip audio = null;
        yield return store.Load<AudioClip>("test.wav", value => audio = value);
        if (audio == null || audio.channels != 1 || audio.samples != 2205) throw new Exception("Native audio import failed.");
        foreach (var entry in store.Catalog.entries)
        {
            if (string.IsNullOrEmpty(entry.nativePath)) continue;
            TextAsset chunk = null;
            yield return store.Load<TextAsset>(entry.key, value => chunk = value);
            int expected = (int)Math.Min(4 * 1024 * 1024, entry.nativeFileLength - entry.nativeOffset);
            if (chunk == null || chunk.bytes.Length != expected) throw new Exception("Native bundle page length mismatch.");
        }
        Debug.Log("FITD_BUNDLE_RUNTIME_PASS: catalog, compressed HDA import, binary bytes, Unity Texture2D, Unity AudioClip, normalized keys, and traversal rejection");
    }

    private static void Tick()
    {
        try
        {
            if (EditorApplication.timeSinceStartup > deadline) throw new Exception("Bundle smoke test timed out.");
            if (pending != null && !pending.isDone) return;
            pending = null;
            while (routines.Count != 0)
            {
                var current = routines.Peek();
                if (!current.MoveNext()) { (current as IDisposable)?.Dispose(); routines.Pop(); continue; }
                var nested = current.Current as IEnumerator;
                if (nested != null) { routines.Push(nested); continue; }
                pending = current.Current as UnityEngine.AsyncOperation;
                return;
            }
            Finish(0);
        }
        catch (Exception error) { Debug.LogException(error); Finish(1); }
    }

    private static void Finish(int status)
    {
        EditorApplication.update -= Tick;
        while (routines.Count != 0) (routines.Pop() as IDisposable)?.Dispose();
        store?.Dispose(); EditorApplication.Exit(status);
    }
}
