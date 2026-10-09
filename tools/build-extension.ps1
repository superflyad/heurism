param([Parameter(Mandatory=$true)][string]$Clang, [Parameter(Mandatory=$true)][string]$Linker)
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$cc = (Resolve-Path -LiteralPath $Clang).Path
$ld = (Resolve-Path -LiteralPath $Linker).Path
$build = Join-Path $repo 'build/extension'
New-Item -ItemType Directory -Force -Path $build | Out-Null
$flags = @('--target=x86_64-pc-windows-msvc', '-std=c11', '-ffreestanding', '-fshort-wchar',
    '-mno-red-zone', '-fno-stack-protector', '-fno-builtin', '-Wall', '-Wextra', '-Werror', '-O2')
& $cc @flags '-c' (Join-Path $repo 'boot/extension.c') '-o' "$build/extension.obj"
if ($LASTEXITCODE -ne 0) { throw 'Extension compilation failed' }
& $ld '/subsystem:efi_boot_service_driver' '/entry:efi_main' '/nodefaultlib' '/machine:x64' '/dynamicbase' '/timestamp:0' "/out:$build/companionextx64.efi" "$build/extension.obj"
if ($LASTEXITCODE -ne 0) { throw 'Extension link failed' }
& $cc @flags '-c' (Join-Path $repo 'tests/extension_host.c') '-o' "$build/test.obj"
if ($LASTEXITCODE -ne 0) { throw 'Extension test compilation failed' }
& $ld '/dll' '/noentry' '/nodefaultlib' '/machine:x64' '/export:extension_run_tests' "/out:$build/extension-tests.dll" "$build/test.obj"
if ($LASTEXITCODE -ne 0) { throw 'Extension test link failed' }
& python (Join-Path $repo 'tests/verify-extension.py') $build
if ($LASTEXITCODE -ne 0) { throw 'Extension validation failed' }
& (Join-Path $PSScriptRoot 'build-firmware-probe.ps1') -ExtensionProbe -Clang $cc -Linker $ld
if ($LASTEXITCODE -ne 0) { throw 'Observer validation failed' }
