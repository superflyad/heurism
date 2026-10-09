param([string]$Clang, [string]$Linker, [switch]$Test)
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
function Find-Tool([string]$Name, [string]$Explicit) {
    if ($Explicit) { return (Resolve-Path -LiteralPath $Explicit).Path }
    $found = Get-Command $Name -ErrorAction SilentlyContinue
    if ($found) { return $found.Source }
    $vsRoots = @("${env:ProgramFiles}\Microsoft Visual Studio", "${env:ProgramFiles(x86)}\Microsoft Visual Studio")
    foreach ($root in $vsRoots) {
        if (Test-Path -LiteralPath $root) {
            $candidate = Get-ChildItem -LiteralPath $root -Recurse -Filter $Name -ErrorAction SilentlyContinue |
                Where-Object { $_.FullName -match 'Llvm\\x64\\bin' } | Select-Object -First 1
            if ($candidate) { return $candidate.FullName }
        }
    }
    throw "Install LLVM clang and lld-link, or supply -Clang and -Linker. Missing: $Name"
}
$cc = Find-Tool 'clang.exe' $Clang
$ld = Find-Tool 'lld-link.exe' $Linker
$build = Join-Path $repo 'build'
$esp = Join-Path $build 'esp\EFI\BOOT'
New-Item -ItemType Directory -Force -Path $esp | Out-Null
$flags = @('--target=x86_64-pc-windows-msvc', '-std=c11', '-ffreestanding', '-fshort-wchar',
    '-mno-red-zone', '-fno-stack-protector', '-fno-builtin', '-Wall', '-Wextra', '-Werror', '-O2')
foreach ($source in @('boot/main.c', 'boot/framebuffer_gop.c', 'ui/framebuffer.c')) {
    $object = Join-Path $build ((Split-Path $source -Leaf) + '.obj')
    & $cc @flags '-c' (Join-Path $repo $source) '-o' $object
    if ($LASTEXITCODE -ne 0) { throw "Compilation failed: $source" }
}
$efi = Join-Path $esp 'BOOTX64.EFI'
& $ld '/subsystem:efi_application' '/entry:efi_main' '/nodefaultlib' '/machine:x64' '/dynamicbase' '/timestamp:0' "/out:$efi" (Join-Path $build 'main.c.obj') (Join-Path $build 'framebuffer_gop.c.obj') (Join-Path $build 'framebuffer.c.obj')
if ($LASTEXITCODE -ne 0) { throw 'EFI link failed' }
if ($Test) {
    & $cc @flags '-c' (Join-Path $repo 'tests/host.c') '-o' (Join-Path $build 'host.obj')
    if ($LASTEXITCODE -ne 0) { throw 'Test compilation failed' }
    & $ld '/dll' '/noentry' '/nodefaultlib' '/machine:x64' '/export:run_tests' '/export:companion_screen' "/out:$build\tests.dll" (Join-Path $build 'host.obj') (Join-Path $build 'main.c.obj') (Join-Path $build 'framebuffer_gop.c.obj') (Join-Path $build 'framebuffer.c.obj')
    if ($LASTEXITCODE -ne 0) { throw 'Test link failed' }
    & python (Join-Path $repo 'tests/verify.py') $efi (Join-Path $build 'tests.dll')
    if ($LASTEXITCODE -ne 0) { throw 'Verification failed' }
}
$hash = (Get-FileHash -LiteralPath $efi -Algorithm SHA256).Hash
"EFI application: $efi"
"SHA256: $hash"
"Copy the contents of build\esp to a prepared FAT32 USB volume."
