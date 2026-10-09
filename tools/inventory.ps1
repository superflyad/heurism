param([string]$OutputPath)
$ErrorActionPreference = 'Stop'
if (!$OutputPath) { $OutputPath = Join-Path (Split-Path $PSScriptRoot -Parent) 'artifacts\hardware-inventory.json' }
$parent = Split-Path $OutputPath -Parent
if ($parent) { New-Item -ItemType Directory -Force -Path $parent | Out-Null }
# Deliberately omit serial numbers, service tags, MAC addresses and user identity.
$inventory = [ordered]@{
    capturedUtc = (Get-Date).ToUniversalTime().ToString('o')
    computer = @(Get-CimInstance Win32_ComputerSystem | Select-Object Manufacturer, Model, SystemType)
    firmware = @(Get-CimInstance Win32_BIOS | Select-Object Manufacturer, SMBIOSBIOSVersion, ReleaseDate)
    cpu = @(Get-CimInstance Win32_Processor | Select-Object Name, NumberOfCores, NumberOfLogicalProcessors)
    memory = @(Get-CimInstance Win32_PhysicalMemory | Select-Object Manufacturer, Capacity, Speed)
    graphics = @(Get-CimInstance Win32_VideoController | Select-Object Name, PNPDeviceID, DriverVersion)
    storage = @(Get-CimInstance Win32_DiskDrive | Select-Object Model, Size, InterfaceType)
    network = @(Get-CimInstance Win32_NetworkAdapter | Where-Object PhysicalAdapter | Select-Object Name, PNPDeviceID)
    pci = @(Get-CimInstance Win32_PnPEntity | Where-Object { $_.PNPDeviceID -like 'PCI\*' } | Select-Object Name, PNPDeviceID)
}
$inventory | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $OutputPath -Encoding utf8
"Saved inventory for $($inventory.computer[0].Model): $OutputPath"
