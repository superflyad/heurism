param([Parameter(Mandatory)][string]$Address, [string]$Command = 'companion-status')
$ErrorActionPreference = 'Stop'
if ($Address -notmatch '^[A-Za-z0-9.:_-]+$' -or $Address.StartsWith('-')) { throw 'Invalid host address' }
$repo = Split-Path $PSScriptRoot -Parent
$key = Join-Path $repo 'artifacts\ssh\companion_client_ed25519'
$known = Join-Path $repo 'artifacts\ssh\known_hosts'
& ssh '-i' $key '-o' 'BatchMode=yes' '-o' 'ConnectTimeout=10' '-o' 'StrictHostKeyChecking=yes' '-o' "UserKnownHostsFile=$known" '-o' 'HostKeyAlias=companion-dell' "root@$Address" $Command
if ($LASTEXITCODE -ne 0) { throw 'SSH command failed. Check the Dell address, network and provisioning boot.' }
