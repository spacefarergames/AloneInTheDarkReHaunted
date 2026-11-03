using System;
using System.Globalization;
using System.IO;
using UnityEditor.Build;

namespace Fitd.Unity
{
    public sealed class FitdBuildStamp : BuildPlayerProcessor
    {
        public override void PrepareForBuild(BuildPlayerContext context)
        {
            string path = Path.GetFullPath("Library/FitdBuildTimestamp.txt");
            File.WriteAllText(path, DateTime.UtcNow.ToString("yyyy-MM-dd HH:mm:ss 'UTC'", CultureInfo.InvariantCulture));
            context.AddAdditionalPathToStreamingAssets(path, "FitdBuildTimestamp.txt");
        }
    }
}
