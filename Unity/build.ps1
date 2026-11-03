param(
    [string]$ProjectPath = "$PSScriptRoot/Project",
    [string]$GameData = "",
    [string]$BaseData = "",
    [string]$UnityVersion = '6000.4.5f1',
    [ValidateSet('Release', 'RelWithDebInfo')][string]$Configuration = 'RelWithDebInfo',
    [string]$UnityEditor = 'F:/Unity Editors/6000.4.5f1/Editor/Unity.exe',
    [string]$Game = 'aitd1',
    [switch]$SkipBuild
)
$ErrorActionPreference = 'Stop'
$sourceRoot = (Resolve-Path "$PSScriptRoot/..").Path
$buildPath = Join-Path $sourceRoot 'build/unity-native'
if (!$SkipBuild) {
    & cmake -S $sourceRoot -B $buildPath -G 'Visual Studio 18 2026' -A x64 -DFITD_BUILD_UNITY=ON -DFITD_UNITY_ONLY=ON -DFITD_UNITY_COMMANDS=ON -DFITD_UNITY_BUNDLED_DATA_ONLY=ON
    if ($LASTEXITCODE) { throw 'Unity plugin CMake configuration failed' }
    & cmake --build $buildPath --config $Configuration --target FitdUnity --parallel
    if ($LASTEXITCODE) { throw 'Unity plugin compilation failed' }
}
$plugin = Join-Path $buildPath "Unity/$Configuration/FitdUnity.dll"
if (!(Test-Path -LiteralPath $plugin)) { throw "Native plugin not found: $plugin" }
$assets = Join-Path $ProjectPath 'Assets/Fitd'
$plugins = Join-Path $assets 'Plugins/x86_64'
New-Item -ItemType Directory -Force -Path $plugins | Out-Null
Copy-Item -LiteralPath "$PSScriptRoot/Package/Runtime" -Destination $assets -Recurse -Force
Copy-Item -LiteralPath "$PSScriptRoot/Package/Editor" -Destination $assets -Recurse -Force
Copy-Item -LiteralPath $plugin -Destination $plugins -Force
$symbols = [IO.Path]::ChangeExtension($plugin, '.pdb')
if (Test-Path -LiteralPath $symbols) { Copy-Item -LiteralPath $symbols -Destination $plugins -Force }
# Explicit importer settings prevent Unity trying to load this binary on other platforms.
@'
fileFormatVersion: 2
guid: 2b0876b9a044435fb1060dd4b51b8be9
PluginImporter:
  serializedVersion: 2
  isPreloaded: 0
  isOverridable: 0
  platformData:
  - first:
      Any:
    second:
      enabled: 0
      settings: {}
  - first:
      Editor: Editor
    second:
      enabled: 1
      settings:
        CPU: x86_64
        OS: Windows
  - first:
      Standalone: Win64
    second:
      enabled: 1
      settings:
        CPU: x86_64
  userData:
  assetBundleName:
  assetBundleVariant:
'@ | Set-Content -LiteralPath (Join-Path $plugins 'FitdUnity.dll.meta')
New-Item -ItemType Directory -Force -Path (Join-Path $ProjectPath 'Packages') | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $ProjectPath 'ProjectSettings') | Out-Null
$projectVersionPath = Join-Path $ProjectPath 'ProjectSettings/ProjectVersion.txt'
if (!(Test-Path -LiteralPath $projectVersionPath)) {
    "m_EditorVersion: $UnityVersion" | Set-Content -LiteralPath $projectVersionPath
}
$manifest = Join-Path $ProjectPath 'Packages/manifest.json'
$package = Get-Content -LiteralPath "$PSScriptRoot/Package/package.json" -Raw | ConvertFrom-Json
if (Test-Path -LiteralPath $manifest) { $manifestData = Get-Content -LiteralPath $manifest -Raw | ConvertFrom-Json }
else { $manifestData = [pscustomobject]@{ dependencies = [pscustomobject]@{} } }
if (!$manifestData.dependencies) { $manifestData | Add-Member dependencies ([pscustomobject]@{}) -Force }
foreach ($dependency in $package.dependencies.PSObject.Properties) {
    if (!$manifestData.dependencies.PSObject.Properties[$dependency.Name]) {
        $manifestData.dependencies | Add-Member -NotePropertyName $dependency.Name -NotePropertyValue $dependency.Value
    }
}
$manifestData | ConvertTo-Json -Depth 20 | Set-Content -LiteralPath $manifest
if ($GameData) {
    $dataRoot = (Resolve-Path -LiteralPath $GameData).Path
    if (!(Test-Path -LiteralPath $UnityEditor)) { throw "Unity Editor not found: $UnityEditor" }
    if (!$BaseData -and $Game -eq 'aitd1' -and !(Test-Path -LiteralPath (Join-Path $dataRoot 'LISTBOD2.PAK')) -and (Test-Path -LiteralPath (Join-Path $dataRoot 'Tatou.exe'))) {
        $releaseHash = (Get-FileHash -LiteralPath (Join-Path $dataRoot 'Tatou.exe') -Algorithm SHA256).Hash.Substring(0,12)
        $BaseData = Join-Path $buildPath "release-originals-$releaseHash"
        & python "$PSScriptRoot/tools/extract_release_data.py" (Join-Path $dataRoot 'Tatou.exe') --reference "$sourceRoot/Shipping" --output $BaseData
        if ($LASTEXITCODE) { throw 'Release original-data extraction failed' }
    }
    $unityArguments = @('-batchmode','-nographics','-quit','-projectPath',[IO.Path]::GetFullPath($ProjectPath),'-buildTarget','Win64','-executeMethod','Fitd.Unity.FitdBundleBuilder.BuildCommandLine','-fitdData',$dataRoot,'-fitdGame',$Game,'-fitdTarget','StandaloneWindows64','-logFile',(Join-Path $buildPath 'bundles.log'))
    if ($BaseData) { $unityArguments += @('-fitdBaseData',(Resolve-Path -LiteralPath $BaseData).Path) }
    $quotedArguments = $unityArguments | ForEach-Object { '"' + $_ + '"' }
    $unityProcess = Start-Process -FilePath $UnityEditor -ArgumentList $quotedArguments -WindowStyle Hidden -Wait -PassThru
    if ($unityProcess.ExitCode) { throw 'Unity game bundle build failed; see build/unity-native/bundles.log' }
}
Write-Host "Unity project prepared: $([IO.Path]::GetFullPath($ProjectPath))"
Write-Host 'Open it with Unity, select FITD > Create Player Scene, then press Play.'

