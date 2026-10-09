param([Parameter(Mandatory=$true)][string]$Clang,[Parameter(Mandatory=$true)][string]$Linker)
$ErrorActionPreference='Stop'
$repo=Split-Path $PSScriptRoot -Parent
$cc=(Resolve-Path -LiteralPath $Clang).Path
$ld=(Resolve-Path -LiteralPath $Linker).Path
$build=Join-Path $repo 'build/network-probe'
New-Item -ItemType Directory -Force -Path $build | Out-Null
$flags=@('--target=x86_64-pc-windows-msvc','-std=c11','-ffreestanding','-fshort-wchar','-mno-red-zone','-fno-stack-protector','-fno-builtin','-Wall','-Wextra','-Werror','-O2')
& $cc @flags '-c' (Join-Path $repo 'boot/network_probe.c') '-o' "$build/probe.obj"
if ($LASTEXITCODE -ne 0) { throw 'Network probe compilation failed' }
& $ld '/subsystem:efi_application' '/entry:efi_main' '/nodefaultlib' '/machine:x64' '/dynamicbase' '/timestamp:0' "/out:$build/fvprobex64.efi" "$build/probe.obj"
if ($LASTEXITCODE -ne 0) { throw 'Network probe link failed' }
& $cc @flags '-c' (Join-Path $repo 'tests/network_probe_host.c') '-o' "$build/test.obj"
if ($LASTEXITCODE -ne 0) { throw 'Network tests compilation failed' }
& $ld '/dll' '/noentry' '/nodefaultlib' '/machine:x64' '/export:network_run_tests' "/out:$build/network-tests.dll" "$build/test.obj"
if ($LASTEXITCODE -ne 0) { throw 'Network tests link failed' }
& python (Join-Path $repo 'tests/verify-network-probe.py') $build
if ($LASTEXITCODE -ne 0) { throw 'Network validation failed' }
