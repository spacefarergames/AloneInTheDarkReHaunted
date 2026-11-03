#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
project="${1:-$root/Unity/Project}"
build="${FITD_BUILD_DIR:-$root/build/unity-native-$(uname -s)}"
case "$(uname -s)" in
 Linux) binary=libFitdUnity.so; folder=Linux;;
 Darwin) binary=libFitdUnity.dylib; folder=macOS;;
 *) echo 'Run this script on Linux or macOS; use build.ps1 on Windows.' >&2; exit 1;;
esac
args=(-DFITD_BUILD_UNITY=ON -DFITD_UNITY_ONLY=ON -DFITD_UNITY_COMMANDS=ON -DFITD_UNITY_BUNDLED_DATA_ONLY=ON -DCMAKE_BUILD_TYPE=Release)
if [[ "$(uname -s)" == Darwin ]]; then args+=('-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64'); fi
cmake -S "$root" -B "$build" "${args[@]}"
cmake --build "$build" --target FitdUnity --parallel
mkdir -p "$project/Assets/Fitd/Plugins/$folder"
cp -R "$root/Unity/Package/Runtime" "$root/Unity/Package/Editor" "$project/Assets/Fitd/"
cp "$build/Unity/$binary" "$project/Assets/Fitd/Plugins/$folder/"
python3 "$root/Unity/tools/prepare_dependencies.py" "$project"
echo "Prepared $project. Build target-specific bundles using FITD > Content in Unity."
