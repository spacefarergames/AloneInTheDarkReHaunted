using System;
using UnityEngine;

namespace Fitd.Unity
{
    public enum FitdAssetKind { Binary, Texture, Audio, Font }

    public sealed class FitdBundleCatalog : ScriptableObject
    {
        public int formatVersion = 1;
        public string target;
        public string game;
        public string contentVersion;
        public Entry[] entries = Array.Empty<Entry>();

        [Serializable]
        public struct Entry
        {
            public string key;
            public string bundle;
            public string asset;
            public FitdAssetKind kind;
            public string nativePath;
            public long nativeOffset;
            public long nativeFileLength;
            public string nativeSha256;
        }
    }
}
