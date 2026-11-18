$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$source = Join-Path $repo 'Assets/Animations/ThirdParty'
$archive = Join-Path $repo 'Assets/_Archive/Animations/ThirdParty'
if (!(Test-Path -LiteralPath $source)) { $source = $archive }
$runtime = Join-Path $repo 'Assets/Animations/Characters/Imported'
$mixamo = Join-Path $runtime 'Mixamo'
New-Item -ItemType Directory -Path $mixamo -Force | Out-Null
$pack = Join-Path $source 'Mixamo_Combat/Pro_Rifle_Pack'
$clips = @('idle','idle aiming','idle crouching','idle crouching aiming','jump up','jump loop','jump down',
    'death from the front','death from the back','death crouching headshot front')
foreach ($direction in @('forward','forward left','forward right','left','right','backward','backward left','backward right')) {
    $clips += 'walk ' + $direction
    $clips += 'run ' + $direction
    $clips += 'sprint ' + $direction
    $clips += 'walk crouching ' + $direction
}
foreach ($clip in $clips) {
    Copy-Item -LiteralPath (Join-Path $pack ($clip + '.fbx')) -Destination (Join-Path $mixamo ($clip + '.fbx')) -Force
}
foreach ($clip in @('Rifle_Kneel_Idle','Rifle_Kneel_To_Aim','Prone_Idle','Prone_Firing_Rifle','Prone_Reloading',
    'Rifle_Pull_Out','Rifle_Put_Away','Rifle_Kneel_To_Prone','Rifle_Prone_To_Kneel')) {
    Copy-Item -LiteralPath (Join-Path $source ('Mixamo_Combat/' + $clip + '.fbx')) -Destination (Join-Path $mixamo ($clip + '.fbx')) -Force
}
$quat = Join-Path $runtime 'Quaternius'
New-Item -ItemType Directory -Path $quat -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $source 'Quaternius_UniversalAnimationLibrary/Extracted/Universal Animation Library[Standard]/Unreal-Godot/UAL1_Standard.glb') -Destination $quat -Force
# Preserve all acquisition files outside runtime search/copy folders.
if ($source -ne $archive) {
    if (Test-Path -LiteralPath $archive) { throw 'Archive already exists; refusing merge or overwrite' }
    $resolved = [IO.Path]::GetFullPath($source)
    if (!$resolved.StartsWith($repo + [IO.Path]::DirectorySeparatorChar)) { throw 'Source outside workspace' }
    New-Item -ItemType Directory -Path (Split-Path $archive) -Force | Out-Null
    Move-Item -LiteralPath $resolved -Destination $archive
}
Write-Output "Runtime FBX files: $((Get-ChildItem -LiteralPath $mixamo -Filter '*.fbx').Count)"
