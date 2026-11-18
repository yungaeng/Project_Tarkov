$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$assetRoot = Join-Path $repo 'Assets'
$archive = Join-Path $assetRoot '_Archive'
$entries = [Collections.Generic.List[object]]::new()
function Move-WorkspaceAsset([string]$from,[string]$to) {
    $from = [IO.Path]::GetFullPath($from)
    $to = [IO.Path]::GetFullPath($to)
    if (!$from.StartsWith($repo + [IO.Path]::DirectorySeparatorChar) -or !$to.StartsWith($archive + [IO.Path]::DirectorySeparatorChar)) { throw 'Move outside intended workspace/archive' }
    if (!(Test-Path -LiteralPath $from -PathType Leaf)) { return }
    if (Test-Path -LiteralPath $to) { throw "Refusing to overwrite $to" }
    $hash = (Get-FileHash -LiteralPath $from -Algorithm SHA256).Hash
    New-Item -ItemType Directory -Path (Split-Path $to) -Force | Out-Null
    Move-Item -LiteralPath $from -Destination $to
    $entries.Add(@{from=$from.Substring($repo.Length+1);to=$to.Substring($repo.Length+1);sha256=$hash})
}
$obsolete = @('Jump','Land','Prone','RifleHold','RifleIdle','RifleWalk','RifleRun','RifleCrouchIdle','RifleCrouchWalk')
foreach ($name in $obsolete) {
    $relative = "Animations/Characters/$name.anim"
    Move-WorkspaceAsset (Join-Path $assetRoot $relative) (Join-Path $archive "LegacyAssets/$relative")
}
foreach ($name in @('Idle','Walking','Fast Run','Crouched Walking')) {
    $relative = "Animations/Characters/Locomotion/$name.fbx"
    Move-WorkspaceAsset (Join-Path $assetRoot $relative) (Join-Path $archive "LegacyAssets/$relative")
}
# Runtime copies are generated from the current source list. Preserve stale copies
# instead of allowing removed source assets to remain in a distribution folder.
$builtAssets = Join-Path $repo 'Project_Tarkov/x64/Debug/Assets'
if (Test-Path -LiteralPath $builtAssets) {
    foreach ($file in Get-ChildItem -LiteralPath $builtAssets -File -Recurse) {
        $relative=$file.FullName.Substring($builtAssets.Length+1)
        if (!(Test-Path -LiteralPath (Join-Path $assetRoot $relative) -PathType Leaf) -or
            $relative -match '\.(md|html|zip)$' -or $relative.StartsWith('Animations\ThirdParty\')) {
            Move-WorkspaceAsset $file.FullName (Join-Path $archive "BuildCopies/Debug/$relative")
        }
    }
}
$manifest=Join-Path $archive 'asset-moves-2026-11-19.json'
$previous=@()
if(Test-Path -LiteralPath $manifest) { $previous=@(Get-Content -LiteralPath $manifest -Raw | ConvertFrom-Json) }
@($previous)+@($entries) | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $manifest -Encoding utf8
Write-Output "Archived $($entries.Count) unused/stale files"
