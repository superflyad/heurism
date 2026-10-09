param([string]$SevenZip)
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$downloadDir = Join-Path $repo 'build\provisioning\downloads'
$extractDir = Join-Path $repo 'build\provisioning\alpine'
New-Item -ItemType Directory -Force -Path $downloadDir | Out-Null
$fileName = 'alpine-extended-3.24.2-x86_64.iso'
$url = "https://dl-cdn.alpinelinux.org/alpine/v3.24/releases/x86_64/$fileName"
$iso = Join-Path $downloadDir $fileName
$checksum = Join-Path $downloadDir 'alpine.sha256'
if (!(Test-Path -LiteralPath $iso)) { Invoke-WebRequest -Uri $url -OutFile $iso }
Invoke-WebRequest -Uri "$url.sha256" -OutFile $checksum
$expected = ((Get-Content -LiteralPath $checksum -Raw) -split '\s+')[0]
if ((Get-FileHash -LiteralPath $iso -Algorithm SHA256).Hash -ne $expected) { throw 'Official ISO checksum mismatch' }
if (!$SevenZip) {
    $candidate = Get-Command 7z.exe -ErrorAction SilentlyContinue
    if ($candidate) { $SevenZip = $candidate.Source }
    else {
        $SevenZip = 'C:\Program Files\Microsoft Visual Studio\18\Enterprise\Common7\IDE\Extensions\Microsoft\Maui\Maui.VisualStudio\7-Zip\7z.exe'
    }
}
if (!(Test-Path -LiteralPath $SevenZip)) { throw 'Supply -SevenZip with a 7z.exe path.' }
& $SevenZip x $iso "-o$extractDir" '-y' '-bso0' '-bsp0'
if ($LASTEXITCODE -ne 0) { throw 'ISO extraction failed' }
if (!(Test-Path -LiteralPath (Join-Path $repo 'build\esp\EFI\BOOT\BOOTX64.EFI'))) {
    & (Join-Path $repo 'tools\build.ps1') -Test
}
& python (Join-Path $repo 'tools\build-provisioning.py')
if ($LASTEXITCODE -ne 0) { throw 'Payload assembly failed' }
& python (Join-Path $repo 'tools\make-fat-image.py') (Join-Path $repo 'build\provisioning\payload') (Join-Path $repo 'build\provisioning\companion-usb.img')
if ($LASTEXITCODE -ne 0) { throw 'Test image assembly failed' }
