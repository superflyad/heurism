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
    throw 'Target must be the specified non-system FAT32 USB.'
}
& python (Join-Path $repo 'tools\publish-provisioning.py') "${DriveLetter}:\"
if ($LASTEXITCODE -ne 0) { throw 'USB publication failed' }
Set-Volume -DriveLetter $DriveLetter -NewFileSystemLabel 'COMPANION'
'Provisioning USB prepared. Safely eject before moving it to the Dell.'
