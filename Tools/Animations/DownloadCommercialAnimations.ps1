$ErrorActionPreference = 'Stop'
$root = Join-Path $PSScriptRoot '../../Assets/_Archive/Animations/ThirdParty'
New-Item -ItemType Directory -Path $root -Force | Out-Null
$packs = @(
    @{Slug='universal-animation-library'; Author='quaternius'; Folder='Quaternius_UniversalAnimationLibrary'},
    @{Slug='universal-animation-library-2'; Author='quaternius'; Folder='Quaternius_UniversalAnimationLibrary2'},
    @{Slug='kaykit-character-animations'; Author='kaylousberg'; Folder='KayKit_CharacterAnimations'}
)
foreach ($pack in $packs) {
    $target = Join-Path $root $pack.Folder
    New-Item -ItemType Directory -Path $target -Force | Out-Null
    if (Test-Path -LiteralPath (Join-Path $target 'original.zip')) { Write-Output "Already downloaded: $($pack.Folder)"; continue }
    $base = 'https://' + $pack.Author + '.itch.io/' + $pack.Slug
    $page = Invoke-WebRequest -Uri $base -SessionVariable animWebSession
    $csrf = [regex]::Match($page.Content, 'name="csrf_token" value="([^"]+)"').Groups[1].Value
    if (!$csrf) { throw 'Missing CSRF token' }
    $link = Invoke-RestMethod -Uri "$base/download_url" -Method Post -WebSession $animWebSession -Body @{csrf_token=$csrf}
    $downloadPage = Invoke-WebRequest -Uri $link.url -WebSession $animWebSession
    $csrf = [regex]::Match($downloadPage.Content, 'name="csrf_token" value="([^"]+)"').Groups[1].Value
    $upload = [regex]::Match($downloadPage.Content, 'data-upload_id="([0-9]+)"').Groups[1].Value
    if (!$upload) { throw 'No free upload found' }
    $file = Invoke-RestMethod -Uri "$base/file/$upload" -Method Post -WebSession $animWebSession -Body @{csrf_token=$csrf}
    if (!$file.url) { throw 'No download URL returned' }
    Invoke-WebRequest -Uri $file.url -OutFile (Join-Path $target 'original.zip')
    Expand-Archive -LiteralPath (Join-Path $target 'original.zip') -DestinationPath (Join-Path $target 'Extracted')
    $page.Content | Set-Content -LiteralPath (Join-Path $target 'source-page.html') -Encoding utf8
    @{ source=$base; retrieved='2026-11-19'; license='CC0-1.0'; uploadId=$upload; sha256=(Get-FileHash -LiteralPath (Join-Path $target 'original.zip') -Algorithm SHA256).Hash } | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $target 'provenance.json') -Encoding utf8
    Write-Output "Downloaded: $($pack.Folder)"
    Start-Sleep -Seconds 5
}
