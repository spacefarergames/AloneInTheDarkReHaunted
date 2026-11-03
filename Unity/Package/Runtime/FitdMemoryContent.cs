using System;
using System.Collections;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using UnityEngine;

namespace Fitd.Unity
{
    // Unity opens the bundles on its main thread. The native readers ask for
    // individual pages, which stay in a bounded RAM cache until evicted.
    internal sealed class FitdMemoryContent
    {
        private const int PageSize = 4 * 1024 * 1024;
        private readonly FitdBundleStore store;
        private FitdBundleCatalog.Entry[][] files;

        [DllImport(FitdPlayer.Library, CallingConvention = CallingConvention.Cdecl)]
        private static extern int FitdUnity_RegisterMemoryFile(
            [MarshalAs(UnmanagedType.LPUTF8Str)] string name, int id, ulong length);

        [DllImport(FitdPlayer.Library, CallingConvention = CallingConvention.Cdecl)]
        private static extern int FitdUnity_GetMemoryRequest(out int file, out int index, out int expected);

        [DllImport(FitdPlayer.Library, CallingConvention = CallingConvention.Cdecl)]
        private static extern void FitdUnity_CompleteMemoryRequest(int file, int index, [In] byte[] bytes, int length);

        public FitdMemoryContent(FitdBundleStore store) { this.store = store; }

        public IEnumerator Prepare(bool register)
        {
            files = store.Catalog.entries
                .Where(entry => !string.IsNullOrEmpty(entry.nativePath))
                .GroupBy(entry => FitdBundleStore.NormalizeKey(entry.nativePath), StringComparer.Ordinal)
                .OrderBy(group => group.Key, StringComparer.Ordinal)
                .Select(group => group.OrderBy(entry => entry.nativeOffset).ToArray()).ToArray();
            if (files.Length == 0) throw new InvalidDataException("The game bundle has no native files.");

            for (int file = 0; file < files.Length; file++)
            {
                var entries = files[file];
                long length = entries[0].nativeFileLength;
                if (length <= 0 || entries.LongLength != (length - 1) / PageSize + 1)
                    throw new InvalidDataException("Invalid game file length: " + entries[0].nativePath);
                for (int page = 0; page < entries.Length; page++)
                {
                    if (entries[page].nativeOffset != (long)page * PageSize || entries[page].nativeFileLength != length)
                        throw new InvalidDataException("Invalid game file page: " + entries[page].nativePath);
                }
                if (register && FitdUnity_RegisterMemoryFile(entries[0].nativePath, file, (ulong)length) == 0)
                    throw new InvalidDataException("Could not register " + entries[0].nativePath);
            }
            yield return store.PreloadNativeBundles();
        }

        public void Pump()
        {
            for (int i = 0; i < 4 && FitdUnity_GetMemoryRequest(out int file, out int index, out int expected) != 0; i++)
            {
                TextAsset asset = null;
                try
                {
                    asset = store.LoadNativeChunk(files[file][index]);
                    var bytes = asset.bytes;
                    if (bytes.Length != expected) throw new InvalidDataException("Game file page size mismatch.");
                    FitdUnity_CompleteMemoryRequest(file, index, bytes, bytes.Length);
                }
                catch
                {
                    FitdUnity_CompleteMemoryRequest(file, index, null, 0);
                    throw;
                }
                finally
                {
                    if (asset != null) Resources.UnloadAsset(asset);
                }
            }
        }
    }
}
