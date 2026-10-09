param([Parameter(Mandatory=$true)][string]$Clang,[Parameter(Mandatory=$true)][string]$Linker)
$ErrorActionPreference='Stop'
$repo=Split-Path $PSScriptRoot -Parent
$cc=(Resolve-Path -LiteralPath $Clang).Path
$ld=(Resolve-Path -LiteralPath $Linker).Path
$build=Join-Path $repo 'build/udp-probe'
New-Item -ItemType Directory -Force -Path $build | Out-Null
& python (Join-Path $PSScriptRoot 'prepare-nv-payload.py')
if ($LASTEXITCODE -ne 0) { throw 'Pinned payload verification failed' }
$flags=@('--target=x86_64-pc-windows-msvc','-std=c11','-ffreestanding','-fshort-wchar','-mno-red-zone','-fno-stack-protector','-fno-builtin','-Wall','-Wextra','-Werror','-O2')
& $cc @flags '-c' (Join-Path $repo 'boot/udp_transfer_probe.c') '-o' "$build/probe.obj"
if ($LASTEXITCODE -ne 0) { throw 'UDP probe compilation failed' }
& $ld '/subsystem:efi_application' '/entry:efi_main' '/nodefaultlib' '/machine:x64' '/dynamicbase' '/timestamp:0' "/out:$build/fvprobex64.efi" "$build/probe.obj"
if ($LASTEXITCODE -ne 0) { throw 'UDP probe link failed' }
& $cc @flags '-c' (Join-Path $repo 'tests/udp_transfer_host.c') '-o' "$build/test.obj"
if ($LASTEXITCODE -ne 0) { throw 'UDP tests compilation failed' }
& $ld '/dll' '/noentry' '/nodefaultlib' '/machine:x64' '/export:udp_run_tests' "/out:$build/udp-tests.dll" "$build/test.obj"
if ($LASTEXITCODE -ne 0) { throw 'UDP tests link failed' }
& python (Join-Path $repo 'tests/verify-udp-probe.py') $build
if ($LASTEXITCODE -ne 0) { throw 'UDP validation failed' }
