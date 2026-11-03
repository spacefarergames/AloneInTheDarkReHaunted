using System;
using System.IO;
using UnityEditor;
using UnityEditor.Build;
using UnityEditor.Build.Reporting;

namespace Fitd.Unity
{
    // Configure only our named native libraries; never change other project plugins.
    public sealed class FitdBuildValidation : AssetPostprocessor, IPreprocessBuildWithReport
    {
        public int callbackOrder => 0;
        void OnPreprocessAsset()
        {
            if (Path.GetFileNameWithoutExtension(assetPath) != "FitdUnity" && Path.GetFileNameWithoutExtension(assetPath) != "libFitdUnity") return;
            var importer = assetImporter as PluginImporter;
            if (importer == null) return;
            string extension = Path.GetExtension(assetPath).ToLowerInvariant();
            BuildTarget target; string os;
            if (extension == ".dll") { target = BuildTarget.StandaloneWindows64; os = "Windows"; }
            else if (extension == ".so") { target = BuildTarget.StandaloneLinux64; os = "Linux"; }
            else if (extension == ".dylib") { target = BuildTarget.StandaloneOSX; os = "OSX"; }
            else return;
            importer.SetCompatibleWithAnyPlatform(false);
            importer.SetCompatibleWithEditor(true);
            importer.SetEditorData("OS", os);
            importer.SetEditorData("CPU", extension == ".dylib" ? "AnyCPU" : "x86_64");
            importer.SetCompatibleWithPlatform(target, true);
        }
        public void OnPreprocessBuild(BuildReport report)
        {
            string binary;
            switch (report.summary.platform)
            {
                case BuildTarget.StandaloneWindows64: binary="FitdUnity.dll"; break;
                case BuildTarget.StandaloneLinux64: binary="libFitdUnity.so"; break;
                case BuildTarget.StandaloneOSX: binary="libFitdUnity.dylib"; break;
                default: throw new BuildFailedException("FITD native gameplay currently supports 64-bit Windows, Linux and macOS desktop builds.");
            }
            bool found=false;
            foreach (var importer in PluginImporter.GetAllImporters())
                if(Path.GetFileName(importer.assetPath)==binary && importer.GetCompatibleWithPlatform(report.summary.platform)) found=true;
            if(!found)throw new BuildFailedException("Build and stage " + binary + " using Unity/build.ps1 or Unity/build_native.sh first.");
            string catalog=Path.Combine("Assets/StreamingAssets/Fitd",report.summary.platform.ToString(),"fitd-catalog");
            if(!File.Exists(catalog))throw new BuildFailedException("Build FITD content bundles for " + report.summary.platform + " first (FITD > Content).");
        }
    }
}
