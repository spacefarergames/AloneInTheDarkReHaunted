using System;
using System.Collections.Generic;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Security.Cryptography;
using System.Text;
using UnityEditor;
using UnityEngine;

namespace Fitd.Unity
{
    public static class FitdBundleBuilder
    {
        private static string Generated => importingJack ? "Assets/FitdJackContent" : "Assets/FitdContent";
        private static bool importingJack;
        private sealed class Source {
            public string key, path, nativePath, nativeSha256;
            public long nativeOffset, nativeFileLength;
        }
        private sealed class ArchiveEntry { public string key; public long offset, size, expanded; public uint flags; }

        [MenuItem("FITD/Content/Import and Build Bundles for Active Platform")]
        public static void BuildFromFolder()
        {
            string folder = EditorUtility.OpenFolderPanel("FITD game data folder", Directory.GetCurrentDirectory(), "");
            if (folder.Length != 0) Build(folder, EditorUserBuildSettings.activeBuildTarget, "aitd1");
        }

        // Batch entry point: -executeMethod Fitd.Unity.FitdBundleBuilder.BuildCommandLine
        // -fitdData <folder> [-fitdGame aitd1] [-fitdTarget StandaloneWindows64]
        public static void BuildCommandLine()
        {
            var arguments = Environment.GetCommandLineArgs();
            Func<string, string, string> option = (name, fallback) => {
                int index = Array.IndexOf(arguments, name);
                return index < 0 ? fallback : index + 1 < arguments.Length ? arguments[index + 1] : throw new ArgumentException("Missing " + name);
            };
            string source = option("-fitdData", null);
            if (source == null) throw new ArgumentException("Pass -fitdData <game data folder>.");
            var target = (BuildTarget)Enum.Parse(typeof(BuildTarget), option("-fitdTarget", EditorUserBuildSettings.activeBuildTarget.ToString()));
            Build(source, target, option("-fitdGame", "aitd1"),option("-fitdBaseData",null));
        }

        // Retry packaging from a completed import without unpacking/hashing the source again.
        public static void BuildImportedCommandLine()
        {
            BuildImported("aitd1");
        }

        public static void BuildImported(string game)
        {
            importingJack = game == "jack";
            try { BuildImportedGame(game); }
            finally { importingJack = false; }
        }

        private static void BuildImportedGame(string game)
        {
            var catalog=AssetDatabase.LoadAssetAtPath<FitdBundleCatalog>(Generated+"/catalog.asset");
            var target=EditorUserBuildSettings.activeBuildTarget;
            if(catalog==null || catalog.formatVersion!=1 || catalog.game!=game || catalog.entries.Length==0)
                throw new InvalidDataException("Import game data for the active target first.");
            var definitions=catalog.entries.GroupBy(entry=>entry.bundle).Select(group=>new AssetBundleBuild{
                assetBundleName=group.Key,
                assetNames=group.Select(entry=>ImportPath(entry.key)).ToArray(),
                addressableNames=group.Select(entry=>entry.asset).ToArray()
            }).ToList();
            foreach(var definition in definitions)foreach(var path in definition.assetNames)if(!File.Exists(path))throw new FileNotFoundException("Imported bundle source is missing",path);
            string catalogPath = Generated + "/catalog-" + target + ".asset";
            var targetCatalog = AssetDatabase.LoadAssetAtPath<FitdBundleCatalog>(catalogPath);
            if (targetCatalog == null)
            {
                targetCatalog = ScriptableObject.CreateInstance<FitdBundleCatalog>();
                AssetDatabase.CreateAsset(targetCatalog, catalogPath);
            }
            EditorUtility.CopySerialized(catalog, targetCatalog);
            targetCatalog.target = target.ToString();
            EditorUtility.SetDirty(targetCatalog);
            AssetDatabase.SaveAssets();
            definitions.Add(new AssetBundleBuild{assetBundleName="fitd-catalog",assetNames=new[]{catalogPath},addressableNames=new[]{"catalog"}});
            string output=Path.Combine("Assets/StreamingAssets/Fitd",target.ToString());
            if(importingJack) output=Path.Combine(output,"jack");
            Directory.CreateDirectory(output);
            if(BuildPipeline.BuildAssetBundles(output,definitions.ToArray(),BuildAssetBundleOptions.ChunkBasedCompression|BuildAssetBundleOptions.StrictMode,target)==null)throw new IOException("Unity bundle packaging failed.");
            Debug.Log("FITD_BUNDLES_PASS: "+game+" "+catalog.entries.Length+" assets, "+definitions.Count+" bundles for "+target+"; version "+catalog.contentVersion);
        }

        public static void Build(string sourceFolder, BuildTarget target, string game,string baseDataFolder=null)
        {
            importingJack = game == "jack";
            try { BuildGame(sourceFolder, target, game, baseDataFolder); }
            finally { importingJack = false; }
        }

        private static void BuildGame(string sourceFolder, BuildTarget target, string game, string baseDataFolder)
        {
            sourceFolder = Path.GetFullPath(sourceFolder);
            if (!Directory.Exists(sourceFolder)) throw new DirectoryNotFoundException(sourceFolder);
            if (EditorUserBuildSettings.activeBuildTarget != target)
                throw new InvalidOperationException("Select " + target + " first, or launch Unity with -buildTarget. Bundles must match the active platform.");
            if (!BuildPipeline.IsBuildTargetSupported(BuildPipeline.GetBuildTargetGroup(target), target))
                throw new PlatformNotSupportedException("Install Unity build support for " + target);
            string generatedFull = Path.GetFullPath(Generated);
            if (sourceFolder.StartsWith(generatedFull + Path.DirectorySeparatorChar, StringComparison.OrdinalIgnoreCase) || sourceFolder == generatedFull)
                throw new ArgumentException("The import source cannot be the generated content directory.");
            // Everything below Generated is owned by this importer, never user-authored assets.
            string marker = Path.Combine(Generated, ".fitd-generated");
            if (Directory.Exists(Generated) && !File.Exists(marker))
                throw new IOException(Generated + " already exists without an importer ownership marker.");
            Directory.CreateDirectory(Generated);
            File.WriteAllText(marker, "FITD generated bundle inputs v1\n");
            var sources = new Dictionary<string, Source>(StringComparer.OrdinalIgnoreCase);
            if(baseDataFolder!=null){
                baseDataFolder=Path.GetFullPath(baseDataFolder);
                foreach(var file in Directory.GetFiles(baseDataFolder).Where(Supported).OrderBy(path=>path,StringComparer.Ordinal))
                    ImportNativeFile(file,Path.GetFileName(file),sources);
            }
            foreach (var archive in Directory.GetFiles(sourceFolder).Where(path => Path.GetExtension(path).Equals(".hda", StringComparison.OrdinalIgnoreCase)).OrderBy(path => path, StringComparer.Ordinal))
            {
                string name = Path.GetFileNameWithoutExtension(archive).ToLowerInvariant();
                if (name.Contains("backup") || name.Contains("_v1_")) continue;
                Debug.Log("FITD_IMPORT: extracting "+Path.GetFileName(archive));
                ExtractArchive(archive, name, sources);
                Debug.Log("FITD_IMPORT: extracted "+Path.GetFileName(archive));
                ImportNativeFile(archive, Path.GetFileName(archive), sources);
            }
            foreach (var file in Directory.GetFiles(sourceFolder, "*", SearchOption.AllDirectories).OrderBy(path => path, StringComparer.Ordinal))
            {
                string relative = file.Substring(sourceFolder.TrimEnd(Path.DirectorySeparatorChar).Length + 1).Replace('\\', '/');
                string lower = relative.ToLowerInvariant();
                if (lower.Split('/').Any(part => part.StartsWith("_") || part == "backups" || part.Contains("backup") || part.EndsWith("_dump"))) continue;
                if (System.Text.RegularExpressions.Regex.IsMatch(Path.GetFileName(lower), @"^save\d")) continue;
                if (!Supported(file)) continue;
                string key = FitdBundleStore.NormalizeKey(relative);
                string destination = ImportPath(key);
                Directory.CreateDirectory(Path.GetDirectoryName(destination));
                if (!File.Exists(destination) || new FileInfo(file).Length != new FileInfo(destination).Length || FileHash(file) != FileHash(destination))
                    File.Copy(file, destination, true); // Loose overrides take priority over packed data.
                var imported = new Source { key = key, path = destination.Replace('\\', '/') };
                sources[key] = imported;
                if (Path.GetExtension(destination) == ".bytes" && new FileInfo(file).Length <= 4*1024*1024)
                {
                    imported.nativePath = relative;
                    imported.nativeFileLength = new FileInfo(file).Length;
                    imported.nativeSha256 = FileHash(file);
                }
                else ImportNativeFile(file, relative, sources);
            }
            if (sources.Count == 0) throw new InvalidDataException("No supported game content found in " + sourceFolder);
            AssetDatabase.Refresh(ImportAssetOptions.ForceSynchronousImport);
            Debug.Log("FITD_IMPORT: configuring "+sources.Count+" assets");
            var catalogEntries = new List<FitdBundleCatalog.Entry>();
            var groups = new Dictionary<string, List<Source>>();
            // Queue importer changes together; refreshing the entire project for
            // each texture makes large replacement packs needlessly expensive.
            AssetDatabase.StartAssetEditing();
            try {
            foreach (var source in sources.Values.OrderBy(value => value.key, StringComparer.Ordinal))
            {
                if(catalogEntries.Count%500==0)Debug.Log("FITD_IMPORT: settings "+catalogEntries.Count+"/"+sources.Count);
                FitdAssetKind kind = ConfigureImporter(source.path);
                string bundle = BundleName(source.key, kind);
                if (!groups.ContainsKey(bundle)) groups.Add(bundle, new List<Source>());
                groups[bundle].Add(source);
                catalogEntries.Add(new FitdBundleCatalog.Entry {
                    key = source.key, asset = source.key, bundle = bundle, kind = kind,
                    nativePath = source.nativePath, nativeOffset = source.nativeOffset,
                    nativeFileLength = source.nativeFileLength, nativeSha256 = source.nativeSha256
                });
            }
            } finally { AssetDatabase.StopAssetEditing(); }
            AssetDatabase.Refresh(ImportAssetOptions.ForceSynchronousImport);
            string catalogPath = Generated + "/catalog.asset";
            var catalog = AssetDatabase.LoadAssetAtPath<FitdBundleCatalog>(catalogPath);
            if (catalog == null) { catalog = ScriptableObject.CreateInstance<FitdBundleCatalog>(); AssetDatabase.CreateAsset(catalog, catalogPath); }
            catalog.target = target.ToString(); catalog.game = game;
            catalog.entries = catalogEntries.ToArray();
            using (var hash = SHA256.Create())
            {
                foreach (var source in sources.Values.OrderBy(value => value.key, StringComparer.Ordinal))
                {
                    byte[] label = Encoding.UTF8.GetBytes(source.key + "\n"); hash.TransformBlock(label, 0, label.Length, null, 0);
                    using (var input = File.OpenRead(source.path))
                    {
                        var buffer = new byte[65536]; int count;
                        while ((count = input.Read(buffer, 0, buffer.Length)) != 0) hash.TransformBlock(buffer, 0, count, null, 0);
                    }
                }
                hash.TransformFinalBlock(Array.Empty<byte>(), 0, 0);
                catalog.contentVersion = BitConverter.ToString(hash.Hash).Replace("-", "").ToLowerInvariant();
            }
            EditorUtility.SetDirty(catalog); AssetDatabase.SaveAssets();
            var definitions = groups.Select(group => new AssetBundleBuild {
                assetBundleName = group.Key,
                assetNames = group.Value.Select(value => value.path).ToArray(),
                addressableNames = group.Value.Select(value => value.key).ToArray()
            }).ToList();
            definitions.Add(new AssetBundleBuild { assetBundleName = "fitd-catalog", assetNames = new[] { catalogPath }, addressableNames = new[] { "catalog" } });
            string output = Path.Combine("Assets/StreamingAssets/Fitd", target.ToString());
            if (importingJack) output = Path.Combine(output, "jack");
            Directory.CreateDirectory(output);
            Debug.Log("FITD_IMPORT: building "+definitions.Count+" bundles");
            var result = BuildPipeline.BuildAssetBundles(output, definitions.ToArray(), BuildAssetBundleOptions.ChunkBasedCompression | BuildAssetBundleOptions.StrictMode, target);
            if (result == null) throw new IOException("Unity failed to build FITD AssetBundles.");
            AssetDatabase.Refresh();
            Debug.Log("FITD_BUNDLES_PASS: " + sources.Count + " assets, " + definitions.Count + " bundles for " + target + "; version " + catalog.contentVersion);
        }

        private static string ImportPath(string key)
        {
            string extension = Path.GetExtension(key).ToLowerInvariant();
            bool native = new[] { ".png", ".jpg", ".jpeg", ".wav", ".mp3", ".ogg", ".ttf", ".otf" }.Contains(extension);
            return Generated + "/data/" + key + (native ? "" : ".bytes");
        }

        private static string FileHash(string file)
        {
            using(var hash=SHA256.Create())using(var input=File.OpenRead(file))return BitConverter.ToString(hash.ComputeHash(input)).Replace("-","").ToLowerInvariant();
        }

        private static void ImportNativeFile(string file, string relative, Dictionary<string,Source> sources)
        {
            string normalized=FitdBundleStore.NormalizeKey(relative), hash=FileHash(file);
            long length=new FileInfo(file).Length;
            using(var input=File.OpenRead(file))
            {
                int chunk=0;long offset=0;
                do {
                    int size=(int)Math.Min(4*1024*1024,length-offset);
                    var data=new byte[size];int position=0,read;
                    while(position<size && (read=input.Read(data,position,size-position))!=0)position+=read;
                    if(position!=size)throw new EndOfStreamException(file);
                    string key="__native/"+normalized+"/"+chunk.ToString("D6");
                    string destination=Generated+"/data/"+key+".bytes";
                    Directory.CreateDirectory(Path.GetDirectoryName(destination));WriteIfChanged(destination,data);
                    sources[key]=new Source {key=key,path=destination,nativePath=relative,nativeOffset=offset,nativeFileLength=length,nativeSha256=hash};
                    offset+=size;chunk++;
                } while(offset<length);
            }
        }

        private static bool Supported(string path)
        {
            return Path.GetFileName(path).Equals("version.txt",StringComparison.OrdinalIgnoreCase) || new[] { ".png", ".jpg", ".jpeg", ".wav", ".mp3", ".ogg", ".ttf", ".otf", ".pak", ".itd", ".dat", ".cfg" }.Contains(Path.GetExtension(path).ToLowerInvariant());
        }

        private static string BundleName(string key, FitdAssetKind kind)
        {
            if (kind == FitdAssetKind.Audio) return "fitd-audio";
            if (kind == FitdAssetKind.Font) return "fitd-fonts";
            if (kind == FitdAssetKind.Texture) return key.StartsWith("atlases/") ? "fitd-atlases" : key.Contains("masks_hd/") ? "fitd-masks" : "fitd-backgrounds";
            return "fitd-data";
        }

        private static void WriteIfChanged(string path,byte[] data)
        {
            if(File.Exists(path) && new FileInfo(path).Length==data.Length && File.ReadAllBytes(path).SequenceEqual(data))return;
            File.WriteAllBytes(path,data);
        }

        private static FitdAssetKind ConfigureImporter(string path)
        {
            string suffix=Path.GetExtension(path).ToLowerInvariant();
            if(suffix==".ttf" || suffix==".otf")return FitdAssetKind.Font;
            if(!new[]{".png",".jpg",".jpeg",".wav",".mp3",".ogg"}.Contains(suffix))return FitdAssetKind.Binary;
            var importer=AssetImporter.GetAtPath(path);
            var texture = importer as TextureImporter;
            if (texture != null)
            {
                bool srgb=!path.Contains("masks_hd/");
                if(!texture.mipmapEnabled && texture.wrapMode==TextureWrapMode.Clamp && texture.maxTextureSize==8192 && !texture.alphaIsTransparency && texture.sRGBTexture==srgb)return FitdAssetKind.Texture;
                texture.mipmapEnabled = false; texture.wrapMode = TextureWrapMode.Clamp;
                texture.maxTextureSize = 8192; texture.alphaIsTransparency = false;
                texture.sRGBTexture = !path.Contains("masks_hd/");
                EditorUtility.SetDirty(texture);AssetDatabase.WriteImportSettingsIfDirty(path);AssetDatabase.ImportAsset(path);return FitdAssetKind.Texture;
            }
            var audio = importer as AudioImporter;
            if (audio != null)
            {
                var previous=audio.defaultSampleSettings;var settings = previous;
                bool needsImport=!audio.loadInBackground;
                settings.loadType = Path.GetExtension(path).Equals(".wav", StringComparison.OrdinalIgnoreCase) ? AudioClipLoadType.DecompressOnLoad : AudioClipLoadType.Streaming;
                settings.sampleRateSetting = AudioSampleRateSetting.PreserveSampleRate;
#if UNITY_6000_0_OR_NEWER
                settings.preloadAudioData = settings.loadType != AudioClipLoadType.Streaming;
#else
                needsImport |= audio.preloadAudioData != (settings.loadType != AudioClipLoadType.Streaming);
                audio.preloadAudioData = settings.loadType != AudioClipLoadType.Streaming;
#endif
                if(!needsImport && settings.Equals(previous))return FitdAssetKind.Audio;
                audio.defaultSampleSettings = settings;
                audio.loadInBackground = true; EditorUtility.SetDirty(audio);AssetDatabase.WriteImportSettingsIfDirty(path);AssetDatabase.ImportAsset(path);return FitdAssetKind.Audio;
            }
            string extension = Path.GetExtension(path).ToLowerInvariant();
            return extension == ".ttf" || extension == ".otf" ? FitdAssetKind.Font : FitdAssetKind.Binary;
        }

        private static void ExtractArchive(string archive, string prefix, Dictionary<string, Source> sources)
        {
            using (var stream = File.OpenRead(archive))
            using (var reader = new BinaryReader(stream, Encoding.UTF8))
            {
                if (reader.ReadUInt32() != 0x47424448) throw new InvalidDataException("Not an HDBG archive: " + archive);
                uint version = reader.ReadUInt32(), count = reader.ReadUInt32();
                if ((version != 1 && version != 2) || count > 1000000) throw new InvalidDataException("Unsupported HDA header.");
                var table = new List<ArchiveEntry>();
                for (uint index = 0; index < count; index++)
                {
                    int length = reader.ReadUInt16();
                    byte[] path = reader.ReadBytes(length);
                    if (path.Length != length) throw new EndOfStreamException();
                    string key = FitdBundleStore.NormalizeKey(prefix + "/" + Encoding.UTF8.GetString(path));
                    ulong offset = reader.ReadUInt64(), size = reader.ReadUInt64();
                    ulong expanded = version == 2 ? reader.ReadUInt64() : size;
                    uint flags = version == 2 ? reader.ReadUInt32() : 0;
                    if (offset > (ulong)stream.Length || size > (ulong)stream.Length - offset || size > 512 * 1024 * 1024 || expanded > 512 * 1024 * 1024 || flags > 1)
                        throw new InvalidDataException("Invalid HDA entry bounds: " + key);
                    table.Add(new ArchiveEntry { key = key, offset = (long)offset, size = (long)size, expanded = (long)expanded, flags = flags });
                }
                long tableEnd = stream.Position;
                foreach (var entry in table)
                {
                    if (entry.offset < tableEnd) throw new InvalidDataException("HDA entry overlaps its table.");
                    if (!Supported(entry.key)) continue;
                    stream.Position = entry.offset;
                    byte[] data = reader.ReadBytes(checked((int)entry.size));
                    if (data.Length != entry.size) throw new EndOfStreamException();
                    if (entry.flags == 1)
                    {
                        if (data.Length < 6 || (data[0] & 15) != 8 || ((data[0] << 8) + data[1]) % 31 != 0 || (data[1] & 32) != 0)
                            throw new InvalidDataException("Invalid zlib header: " + entry.key);
                        using (var compressed = new MemoryStream(data, 2, data.Length - 6))
                        using (var inflater = new DeflateStream(compressed, CompressionMode.Decompress))
                        {
                            byte[] decoded = new byte[checked((int)entry.expanded)]; int position = 0, read;
                            while (position < decoded.Length && (read = inflater.Read(decoded, position, decoded.Length - position)) > 0) position += read;
                            if (position != decoded.Length || inflater.ReadByte() != -1) throw new InvalidDataException("Invalid decompressed size: " + entry.key);
                            uint a = 1, b = 0;
                            // zlib's NMAX keeps both accumulators within uint range
                            // while avoiding two integer divisions for every byte.
                            for(int block=0;block<decoded.Length;block+=5552) {
                                int end=Math.Min(decoded.Length,block+5552);
                                for(int offset=block;offset<end;offset++){a+=decoded[offset];b+=a;}
                                a%=65521;b%=65521;
                            }
                            uint expected = ((uint)data[data.Length-4] << 24) | ((uint)data[data.Length-3] << 16) | ((uint)data[data.Length-2] << 8) | data[data.Length-1];
                            if (((b << 16) | a) != expected) throw new InvalidDataException("Invalid HDA zlib checksum: " + entry.key);
                            data = decoded;
                        }
                    }
                    else if (entry.size != entry.expanded) throw new InvalidDataException("Invalid raw HDA size.");
                    string destination = ImportPath(entry.key);
                    Directory.CreateDirectory(Path.GetDirectoryName(destination)); WriteIfChanged(destination, data);
                    if (sources.ContainsKey(entry.key)) throw new InvalidDataException("Duplicate packed content key: " + entry.key);
                    sources.Add(entry.key, new Source { key = entry.key, path = destination.Replace('\\', '/') });
                }
            }
        }
    }
}
