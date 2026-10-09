param(
    [Parameter(Mandatory)][ValidatePattern('^[A-Za-z]$')][string]$DriveLetter,
    [Parameter(Mandatory)][string]$ExpectedSerial
)
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$partition = Get-Partition -DriveLetter $DriveLetter
$disk = Get-Disk -Number $partition.DiskNumber
$volume = Get-Volume -DriveLetter $DriveLetter
if ($disk.BusType -ne 'USB' -or $disk.IsBoot -or $disk.IsSystem -or
    $disk.SerialNumber.Trim() -ne $ExpectedSerial -or $volume.FileSystem -ne 'FAT32') {
    throw 'Refusing to write: target must be the specified non-system FAT32 USB.'
}
$root = "${DriveLetter}:\"
$source = Join-Path $repo 'build\esp\EFI\BOOT\BOOTX64.EFI'
$sourceHash = (Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$localBackup = Join-Path $repo "artifacts\usb-backup\$stamp"
$usbBackup = Join-Path $root "Companion-backup\$stamp"
New-Item -ItemType Directory -Force -Path $localBackup,$usbBackup | Out-Null
$targets = @('EFI\BOOT\BOOTX64.EFI', 'EFI\Companion\Companion.efi', 'Companion.efi')
$oldFiles = @()
foreach ($relative in $targets) {
    $target = Join-Path $root $relative
    if (Test-Path -LiteralPath $target) {
        $oldHash = (Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash
        foreach ($backupRoot in @($localBackup, $usbBackup)) {
            $backupFile = Join-Path $backupRoot $relative
            New-Item -ItemType Directory -Force -Path (Split-Path $backupFile -Parent) | Out-Null
            Copy-Item -LiteralPath $target -Destination $backupFile
            if ((Get-FileHash -LiteralPath $backupFile -Algorithm SHA256).Hash -ne $oldHash) {
                throw "Backup verification failed: $relative"
            }
        }
        $oldFiles += @{path=$relative; sha256=$oldHash}
    }
}
foreach ($relative in $targets) {
    $target = Join-Path $root $relative
    New-Item -ItemType Directory -Force -Path (Split-Path $target -Parent) | Out-Null
    Copy-Item -LiteralPath $source -Destination $target -Force
    if ((Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash -ne $sourceHash) {
        throw "USB verification failed: $relative"
    }
}
$oldLabel = $volume.FileSystemLabel
$labelChanged = $false
try {
    Set-Volume -DriveLetter $DriveLetter -NewFileSystemLabel 'COMPANION' -ErrorAction Stop
    $labelChanged = (Get-Volume -DriveLetter $DriveLetter).FileSystemLabel -eq 'COMPANION'
} catch { Write-Warning "Image copied, but volume label unchanged: $_" }
@'
COMPANION BOOT 0.2 - x64 UEFI application
Boot this USB's UEFI entry from Dell F12.
Default path: EFI\BOOT\BOOTX64.EFI
Manual boot-file alternatives: Companion.efi or EFI\Companion\Companion.efi

Expected: COMPANION BOOT 0.2 - EFI ENTRY REACHED, then the graphical screen.
If graphics are unsupported, text mode remains active. Press ESC to exit.
Fatal app errors show a stage and status for 15 seconds; photograph them.

This executable is unsigned. Secure Boot must permit it or trust its signing key.
If a security/signature error appears before the banner, changing filenames or
the volume label cannot fix that rejection. Have your BitLocker recovery key
before changing Secure Boot. Do not clear TPM keys or change storage mode.

Previous bootloaders are under Companion-backup. The initial Ubuntu backup has
RESTORE.txt; later backup folders have preparation.json describing their files.
To restore one, copy its saved bootloader over EFI\BOOT\BOOTX64.EFI.
Other Ubuntu files are preserved. There was no format or partition-table change.
'@ | Set-Content -LiteralPath (Join-Path $root 'COMPANION-BOOT.txt') -Encoding utf8
$record = [ordered]@{
    preparedUtc=(Get-Date).ToUniversalTime().ToString('o'); device=$disk.FriendlyName
    drive=$root; previousLabel=$oldLabel; labelChanged=$labelChanged
    sha256=$sourceHash; files=$targets; previousFiles=$oldFiles
    localBackup=$localBackup; usbBackup=$usbBackup; verified=$true
}
$json = $record | ConvertTo-Json -Depth 5
$json | Set-Content -LiteralPath (Join-Path $localBackup 'preparation.json') -Encoding utf8
$json | Set-Content -LiteralPath (Join-Path $usbBackup 'preparation.json') -Encoding utf8
$json
