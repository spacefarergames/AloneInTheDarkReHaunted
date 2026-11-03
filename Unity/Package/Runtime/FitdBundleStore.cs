using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using UnityEngine;
using UnityEngine.Networking;
using Object = UnityEngine.Object;

namespace Fitd.Unity
{
    // Content access remains on Unity's main thread. No native library is needed
    // to load assets, including when StreamingAssets lives inside an Android APK.
    public sealed class FitdBundleStore : IDisposable
    {
        private readonly Dictionary<string, AssetBundle> bundles = new Dictionary<string, AssetBundle>(StringComparer.Ordinal);
        private readonly Dictionary<string, FitdBundleCatalog.Entry> entries = new Dictionary<string, FitdBundleCatalog.Entry>(StringComparer.OrdinalIgnoreCase);
        private readonly HashSet<string> loading = new HashSet<string>(StringComparer.Ordinal);
        private string root;
        private bool disposed;
        public FitdBundleCatalog Catalog { get; private set; }
        public bool Ready { get { return Catalog != null && !disposed; } }

        public static string PlatformName
        {
            get
            {
                switch (Application.platform)
                {
                    case RuntimePlatform.WindowsEditor:
                    case RuntimePlatform.WindowsPlayer: return "StandaloneWindows64";
                    case RuntimePlatform.OSXEditor:
                    case RuntimePlatform.OSXPlayer: return "StandaloneOSX";
                    case RuntimePlatform.LinuxEditor:
                    case RuntimePlatform.LinuxPlayer: return "StandaloneLinux64";
                    case RuntimePlatform.Android: return "Android";
                    case RuntimePlatform.IPhonePlayer: return "iOS";
                    case RuntimePlatform.WebGLPlayer: return "WebGL";
                    default: throw new PlatformNotSupportedException("Unsupported FITD platform: " + Application.platform);
                }
            }
        }

        public IEnumerator Initialize(string baseUri = null)
        {
            if (disposed) throw new ObjectDisposedException(nameof(FitdBundleStore));
            if (Ready) throw new InvalidOperationException("Content store is already initialized.");
            root = (baseUri ?? JoinUri(Application.streamingAssetsPath, "Fitd/" + PlatformName)).TrimEnd('/');
            yield return LoadBundle("fitd-catalog");
            var request = bundles["fitd-catalog"].LoadAssetAsync<FitdBundleCatalog>("catalog");
            yield return request;
            var catalog = request.asset as FitdBundleCatalog;
            if (catalog == null || catalog.formatVersion != 1 || catalog.target != PlatformName)
                throw new InvalidDataException("FITD content catalog is missing, incompatible, or built for another platform.");
            foreach (var entry in catalog.entries)
            {
                string key = NormalizeKey(entry.key);
                if (string.IsNullOrEmpty(entry.bundle) || string.IsNullOrEmpty(entry.asset) || entries.ContainsKey(key))
                    throw new InvalidDataException("Invalid or duplicate FITD catalog entry: " + key);
                entries.Add(key, entry);
            }
            Catalog = catalog;
        }

        public bool Contains(string key) { return Ready && entries.ContainsKey(NormalizeKey(key)); }

        public IEnumerator Load<T>(string key, Action<T> receive) where T : Object
        {
            if (!Ready) throw new InvalidOperationException("Initialize the FITD content store first.");
            if (receive == null) throw new ArgumentNullException(nameof(receive));
            FitdBundleCatalog.Entry entry;
            if (!entries.TryGetValue(NormalizeKey(key), out entry)) throw new FileNotFoundException("FITD bundled asset not found", key);
            if (!bundles.ContainsKey(entry.bundle)) yield return LoadBundle(entry.bundle);
            var request = bundles[entry.bundle].LoadAssetAsync<T>(entry.asset);
            yield return request;
            var asset = request.asset as T;
            if (asset == null) throw new InvalidDataException("Wrong type or missing bundled asset: " + key);
            receive(asset);
        }

        private IEnumerator LoadBundle(string name)
        {
            if (name.IndexOfAny(new[] { '/', '\\', ':' }) >= 0 || name.Contains(".."))
                throw new InvalidDataException("Invalid content bundle name.");
            if(bundles.ContainsKey(name))yield break;
            if(loading.Contains(name))
            {
                while(loading.Contains(name))yield return null;
                if(!bundles.ContainsKey(name))throw new IOException("Another request failed to load FITD bundle "+name);
                yield break;
            }
            foreach(var alreadyLoaded in AssetBundle.GetAllLoadedAssetBundles())
                if(alreadyLoaded.name==name){bundles.Add(name,alreadyLoaded);yield break;}
            loading.Add(name);
            try
            {
                AssetBundle bundle;
                string location=JoinUri(root,name);
                Uri uri;
                if(Uri.TryCreate(location,UriKind.Absolute,out uri) && uri.IsFile)
                {
                    // LZ4 desktop bundles support direct, bounded disk reads.
                    // Avoid downloading the entire native-data bundle into memory.
                    var request=AssetBundle.LoadFromFileAsync(uri.LocalPath);
                    yield return request;
                    bundle=request.assetBundle;
                }
                else
                {
                    using(var request=UnityWebRequestAssetBundle.GetAssetBundle(location))
                    {
                        yield return request.SendWebRequest();
                        if(request.result!=UnityWebRequest.Result.Success)throw new IOException("FITD bundle "+name+": "+request.error);
                        bundle=DownloadHandlerAssetBundle.GetContent(request);
                    }
                }
                if(bundle==null)throw new InvalidDataException("Invalid FITD bundle: "+name);
                if(disposed){bundle.Unload(true);throw new ObjectDisposedException(nameof(FitdBundleStore));}
                if(bundles.ContainsKey(name))bundle.Unload(false);
                else bundles.Add(name,bundle);
            }
            finally {loading.Remove(name);}
        }

        internal IEnumerator PreloadNativeBundles()
        {
            var names=new HashSet<string>();
            foreach(var entry in Catalog.entries)if(!string.IsNullOrEmpty(entry.nativePath) && names.Add(entry.bundle))yield return LoadBundle(entry.bundle);
        }
        internal TextAsset LoadNativeChunk(FitdBundleCatalog.Entry entry)
        {
            AssetBundle bundle;
            if(entry.kind!=FitdAssetKind.Binary || !bundles.TryGetValue(entry.bundle,out bundle))throw new InvalidDataException("Native bundle chunk is not ready.");
            var asset=bundle.LoadAsset<TextAsset>(entry.asset);
            if(asset==null)throw new InvalidDataException("Native bundle chunk is missing: "+entry.key);
            return asset;
        }

        public static string NormalizeKey(string key)
        {
            if (string.IsNullOrWhiteSpace(key)) throw new ArgumentException("Empty content key.");
            string normalized = key.Replace('\\', '/');
            if (normalized.StartsWith("/") || normalized.Contains(":") || Array.Exists(normalized.Split('/'), part => part == ".." || part == "." || part.Length == 0))
                throw new ArgumentException("Content keys must be relative paths without traversal: " + key);
            return normalized.ToLowerInvariant();
        }

        private static string JoinUri(string parent, string child)
        {
            // Preserve jar:file:// and HTTP URLs; a plain disk path needs a file URI.
            string combined = parent.TrimEnd('/', '\\') + "/" + child;
            if (combined.Contains("://") || combined.StartsWith("jar:")) return combined;
            return new Uri(Path.GetFullPath(combined)).AbsoluteUri;
        }

        public void Dispose()
        {
            if (disposed) return;
            disposed = true;
            foreach (var bundle in bundles.Values) bundle.Unload(true);
            bundles.Clear(); entries.Clear(); Catalog = null;
        }
    }
}
