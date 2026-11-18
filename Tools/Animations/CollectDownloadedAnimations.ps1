$ErrorActionPreference = 'Stop'
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$downloads=[IO.Path]::GetFullPath((Join-Path $env:USERPROFILE 'Downloads'))
$archive=[IO.Path]::GetFullPath((Join-Path $repo 'Assets/_Archive/Downloads'))
$runtime=Join-Path $repo 'Assets/Animations/Characters/Imported/Mixamo'
$records=[Collections.Generic.List[object]]::new()
$names=@('Rifle Kneel Idle','Rifle Kneel To Aim','Prone Idle','Prone Firing Rifle','Prone Reloading',
    'Rifle Pull Out','Rifle Put Away','Rifle Kneel To Prone','Rifle Prone To Kneel','Pro Rifle Pack')
foreach($name in $names) {
    $filename=$name+$(if($name-eq'Pro Rifle Pack'){'.zip'}else{'.fbx'})
    $source=[IO.Path]::GetFullPath((Join-Path $downloads $filename))
    $target=[IO.Path]::GetFullPath((Join-Path $archive $filename))
    if(!$source.StartsWith($downloads+[IO.Path]::DirectorySeparatorChar) -or
       !$target.StartsWith($repo+[IO.Path]::DirectorySeparatorChar)) {throw 'Unexpected source or destination'}
    if(!(Test-Path -LiteralPath $source -PathType Leaf)) {continue}
    $expected=if($name-eq'Pro Rifle Pack') {Join-Path $repo 'Assets/_Archive/Animations/ThirdParty/Mixamo_Combat/Pro_Rifle_Pack.zip'}
        else {Join-Path $runtime ($name.Replace(' ','_')+'.fbx')}
    $hash=(Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash
    if($hash-ne(Get-FileHash -LiteralPath $expected -Algorithm SHA256).Hash) {throw "Not the acquired asset: $filename"}
    if(Test-Path -LiteralPath $target) {throw "Archive destination exists: $target"}
    New-Item -ItemType Directory -Path $archive -Force | Out-Null
    Move-Item -LiteralPath $source -Destination $target
    $records.Add(@{name=$filename;sha256=$hash;from=$source;to=$target.Substring($repo.Length+1)})
}
if($records.Count) {$records | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $archive 'collected-2026-11-19.json') -Encoding utf8}
Write-Output "Moved $($records.Count) matching acquisition files out of Downloads"
