param([Parameter(Mandatory=$true)][string]$Clang,[Parameter(Mandatory=$true)][string]$Linker)
$ErrorActionPreference='Stop'
$repo=Split-Path $PSScriptRoot -Parent
$cc=(Resolve-Path -LiteralPath $Clang).Path
$ld=(Resolve-Path -LiteralPath $Linker).Path
$build=Join-Path $repo 'build/nv-startup'
New-Item -ItemType Directory -Force -Path $build | Out-Null
& python (Join-Path $PSScriptRoot 'prepare-nv-payload.py')
if ($LASTEXITCODE -ne 0) { throw 'Pinned payload validation failed' }
$flags=@('--target=x86_64-pc-windows-msvc','-std=c11','-ffreestanding','-fshort-wchar','-mno-red-zone','-fno-stack-protector','-fno-builtin','-Wall','-Wextra','-Werror','-O2')
& $cc @flags '-c' (Join-Path $repo 'boot/nv_startup.c') '-o' "$build/driver.obj"
if ($LASTEXITCODE -ne 0) { throw 'Startup compilation failed' }
& $ld '/subsystem:efi_boot_service_driver' '/entry:efi_main' '/nodefaultlib' '/machine:x64' '/dynamicbase' '/timestamp:0' "/out:$build/companionextx64.efi" "$build/driver.obj"
if ($LASTEXITCODE -ne 0) { throw 'Startup link failed' }
& $cc @flags '-c' (Join-Path $repo 'tests/nv_startup_host.c') '-o' "$build/test.obj"
if ($LASTEXITCODE -ne 0) { throw 'Startup tests compilation failed' }
& $ld '/dll' '/noentry' '/nodefaultlib' '/machine:x64' '/export:nv_startup_run_tests' "/out:$build/nv-startup-tests.dll" "$build/test.obj"
if ($LASTEXITCODE -ne 0) { throw 'Startup tests link failed' }
& python (Join-Path $repo 'tests/verify-nv-startup.py') $build
if ($LASTEXITCODE -ne 0) { throw 'Startup validation failed' }
