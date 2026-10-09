param([Parameter(Mandatory)][string]$Firmware, [string]$Qemu = 'qemu-system-x86_64')
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$esp = Join-Path $repo 'build\esp'
if (!(Test-Path -LiteralPath (Join-Path $esp 'EFI\BOOT\BOOTX64.EFI'))) { throw 'Run tools/build.ps1 first.' }
$rom = (Resolve-Path -LiteralPath $Firmware).Path
# Use a combined x64 OVMF firmware image, not OVMF_CODE alone with missing VARS.
& $Qemu '-machine' 'q35' '-m' '256M' '-bios' $rom '-drive' "format=raw,file=fat:ro:$esp" '-net' 'none'
if ($LASTEXITCODE -ne 0) { throw 'QEMU failed. Check the QEMU executable and combined x64 OVMF image.' }
