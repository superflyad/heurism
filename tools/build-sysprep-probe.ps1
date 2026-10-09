param([string]$Llvm = 'C:\Program Files\Microsoft Visual Studio\18\Enterprise\VC\Tools\Llvm\x64\bin')
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$out = Join-Path $repo 'build\sysprep-probe'
New-Item -ItemType Directory -Force -Path $out | Out-Null
$cc = Join-Path $Llvm 'clang.exe'
$ld = Join-Path $Llvm 'lld-link.exe'
foreach ($name in @('sysprep_probe', 'sysprep_setup_vm')) {
    $obj = Join-Path $out "$name.obj"
    $efi = Join-Path $out "$name.EFI"
    & $cc '--target=x86_64-pc-windows-msvc' '-std=c11' '-ffreestanding' '-fshort-wchar' '-mno-red-zone' '-fno-stack-protector' '-fno-builtin' '-Wall' '-Wextra' '-Werror' '-O2' '-c' (Join-Path $repo "boot\$name.c") '-o' $obj
    if ($LASTEXITCODE -ne 0) { throw "Compile failed: $name" }
    & $ld '/subsystem:efi_application' '/entry:efi_main' '/nodefaultlib' '/machine:x64' '/dynamicbase' '/timestamp:0' "/out:$efi" $obj
    if ($LASTEXITCODE -ne 0) { throw "EFI link failed: $name" }
    "VM-only EFI application: $efi"
    "SHA256: $((Get-FileHash -LiteralPath $efi -Algorithm SHA256).Hash)"
}
