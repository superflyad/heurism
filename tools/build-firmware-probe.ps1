param([string]$Clang, [string]$Linker, [switch]$UpdateProbe, [switch]$PolicyProbe, [switch]$ExtensionProbe, [switch]$HttpInspectProbe, [switch]$HttpPrereqProbe, [switch]$HttpInitProbe, [switch]$HttpEthernetProbe, [switch]$HttpCleanupProbe)
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
function Probe-Tool([string]$Name, [string]$Explicit) {
    if ($Explicit) { return (Resolve-Path -LiteralPath $Explicit).Path }
    $command = Get-Command $Name -ErrorAction SilentlyContinue
    if ($command) { return $command.Source }
    $candidate = Get-ChildItem "${env:ProgramFiles}/Microsoft Visual Studio" -Recurse -Filter $Name -ErrorAction SilentlyContinue |
        Where-Object { $_.FullName -match 'Llvm\\x64\\bin' } | Select-Object -First 1
    if ($candidate) { return $candidate.FullName }
    throw "Missing $Name; supply its path."
}
$cc = Probe-Tool 'clang.exe' $Clang
$ld = Probe-Tool 'lld-link.exe' $Linker
if (([int]$UpdateProbe.IsPresent + [int]$PolicyProbe.IsPresent + [int]$ExtensionProbe.IsPresent + [int]$HttpInspectProbe.IsPresent + [int]$HttpPrereqProbe.IsPresent + [int]$HttpInitProbe.IsPresent + [int]$HttpEthernetProbe.IsPresent + [int]$HttpCleanupProbe.IsPresent) -gt 1) { throw 'Select one probe' }
$name = if ($HttpCleanupProbe) { 'http-cleanup-probe' } elseif ($HttpEthernetProbe) { 'http-ethernet-probe' } elseif ($HttpInitProbe) { 'http-init-probe' } elseif ($HttpPrereqProbe) { 'http-prereq-probe' } elseif ($HttpInspectProbe) { 'http-inspect-probe' } elseif ($ExtensionProbe) { 'extension-probe' } elseif ($PolicyProbe) { 'policy-probe' } elseif ($UpdateProbe) { 'update-probe' } else { 'firmware-probe' }
$source = if ($HttpCleanupProbe) { 'boot/http_cleanup_probe.c' } elseif ($HttpEthernetProbe) { 'boot/http_ethernet_probe.c' } elseif ($HttpInitProbe) { 'boot/http_init_probe.c' } elseif ($HttpPrereqProbe) { 'boot/http_prereq_probe.c' } elseif ($HttpInspectProbe) { 'boot/http_inspect_probe.c' } elseif ($ExtensionProbe) { 'boot/extension_probe.c' } elseif ($PolicyProbe) { 'boot/policy_probe.c' } elseif ($UpdateProbe) { 'boot/update_probe.c' } else { 'boot/firmware_probe.c' }
if ($HttpPrereqProbe -or $HttpInitProbe -or $HttpEthernetProbe -or $HttpCleanupProbe) { & python (Join-Path $repo 'tools/generate-http-prereq-pins.py'); if ($LASTEXITCODE -ne 0) { throw 'Pin generation failed' } }
$build = Join-Path $repo "build/$name"
New-Item -ItemType Directory -Force -Path $build | Out-Null
$flags = @('--target=x86_64-pc-windows-msvc', '-std=c11', '-ffreestanding', '-fshort-wchar',
    '-mno-red-zone', '-fno-stack-protector', '-fno-builtin', '-Wall', '-Wextra', '-Werror', '-O2')
& $cc @flags '-c' (Join-Path $repo $source) '-o' "$build/probe.obj"
if ($LASTEXITCODE -ne 0) { throw 'Probe compilation failed' }
& $ld '/subsystem:efi_application' '/entry:efi_main' '/nodefaultlib' '/machine:x64' '/dynamicbase' '/timestamp:0' "/out:$build/fvprobex64.efi" "$build/probe.obj"
if ($LASTEXITCODE -ne 0) { throw 'Probe link failed' }
[string[]]$testFlags = @()
if ($UpdateProbe) { $testFlags = @('-DTEST_UPDATE_PROBE') }
if ($PolicyProbe) { $testFlags = @('-DTEST_POLICY_PROBE') }
if ($ExtensionProbe) { $testFlags = @('-DTEST_EXTENSION_PROBE') }
if ($HttpPrereqProbe) { $testFlags = @('-DTEST_HTTP_PREREQ_PROBE') }
if ($HttpInitProbe) { $testFlags = @('-DTEST_HTTP_INIT_PROBE') }
if ($HttpEthernetProbe) { $testFlags = @('-DTEST_HTTP_ETHERNET_PROBE') }
if ($HttpCleanupProbe) { $testFlags = @('-DTEST_HTTP_CLEANUP_PROBE') }
if ($HttpInspectProbe) { $testFlags = @('-DTEST_HTTP_INSPECT_PROBE') }
& $cc @flags @testFlags '-c' (Join-Path $repo 'tests/firmware_probe_host.c') '-o' "$build/test.obj"
if ($LASTEXITCODE -ne 0) { throw 'Probe test compilation failed' }
& $ld '/dll' '/noentry' '/nodefaultlib' '/machine:x64' '/export:probe_run_tests' '/export:probe_mock_report' "/out:$build/probe-tests.dll" "$build/test.obj"
if ($LASTEXITCODE -ne 0) { throw 'Probe test link failed' }
& python (Join-Path $repo 'tests/verify-firmware-probe.py') $build
if ($LASTEXITCODE -ne 0) { throw 'Probe validation failed' }
if ($HttpInspectProbe) {
    & $cc @flags '-c' (Join-Path $repo 'tests/http_inspect_host.c') '-o' "$build/inspect-test.obj"
    if ($LASTEXITCODE -ne 0) { throw 'Inspection tests compilation failed' }
    & $ld '/dll' '/noentry' '/nodefaultlib' '/machine:x64' '/export:inspect_run_tests' "/out:$build/inspect-tests.dll" "$build/inspect-test.obj"
    if ($LASTEXITCODE -ne 0) { throw 'Inspection tests link failed' }
    & python (Join-Path $repo 'tests/verify-http-inspect.py') $build
    if ($LASTEXITCODE -ne 0) { throw 'Inspection tests failed' }
}

if ($HttpPrereqProbe) {
 & $cc @flags '-c' (Join-Path $repo 'tests/http_prereq_host.c') '-o' "$build/prereq-test.obj"
 if ($LASTEXITCODE -ne 0) { throw 'Prerequisite test compilation failed' }
 & $ld '/dll' '/noentry' '/nodefaultlib' '/machine:x64' '/export:prereq_run_tests' "/out:$build/prereq-tests.dll" "$build/prereq-test.obj"
 if ($LASTEXITCODE -ne 0) { throw 'Prerequisite test link failed' }
 & python (Join-Path $repo 'tests/verify-http-prereq.py') $build
 if ($LASTEXITCODE -ne 0) { throw 'Prerequisite tests failed' }
}
if ($HttpInitProbe) {
 & $cc @flags '-c' (Join-Path $repo 'tests/http_init_host.c') '-o' "$build/init-test.obj"
 if ($LASTEXITCODE -ne 0) { throw 'Initialization test compilation failed' }
 & $ld '/dll' '/noentry' '/nodefaultlib' '/machine:x64' '/export:init_run_tests' "/out:$build/init-tests.dll" "$build/init-test.obj"
 if ($LASTEXITCODE -ne 0) { throw 'Initialization test link failed' }
 & python (Join-Path $repo 'tests/verify-http-init.py') $build
 if ($LASTEXITCODE -ne 0) { throw 'Initialization tests failed' }
}

if ($HttpEthernetProbe) {
 & $cc @flags '-c' (Join-Path $repo 'tests/http_ethernet_host.c') '-o' "$build/ethernet-test.obj"
 if ($LASTEXITCODE -ne 0) { throw 'Ethernet test compilation failed' }
 & $ld '/dll' '/noentry' '/nodefaultlib' '/machine:x64' '/export:ethernet_run_tests' "/out:$build/ethernet-tests.dll" "$build/ethernet-test.obj"
 if ($LASTEXITCODE -ne 0) { throw 'Ethernet test link failed' }
 & python (Join-Path $repo 'tests/verify-http-ethernet.py') $build
 if ($LASTEXITCODE -ne 0) { throw 'Ethernet tests failed' }
}

if ($HttpCleanupProbe) {
 & $cc @flags '-c' (Join-Path $repo 'tests/http_cleanup_host.c') '-o' "$build/cleanup-test.obj"
 if ($LASTEXITCODE -ne 0) { throw 'Cleanup inspection test compilation failed' }
 & $ld '/dll' '/noentry' '/nodefaultlib' '/machine:x64' '/export:cleanup_run_tests' "/out:$build/cleanup-tests.dll" "$build/cleanup-test.obj"
 if ($LASTEXITCODE -ne 0) { throw 'Cleanup inspection test link failed' }
 & python (Join-Path $repo 'tests/verify-http-cleanup.py') $build
 if ($LASTEXITCODE -ne 0) { throw 'Cleanup inspection tests failed' }
}
