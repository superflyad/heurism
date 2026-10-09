$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$package = Join-Path $repo 'artifacts/firmware/vendor/BIOS_IMG-1.35.0.rcv'
$digest = (Get-FileHash -LiteralPath $package -Algorithm SHA256).Hash.ToLowerInvariant()
if ($digest -ne 'e649ae3fc5684a7bc7790bc3f2dca7f4235986d80af469ec60b926fc0e7fa9ed') {
    throw 'Recovery package differs from Dell published checksum'
}
$signature = Get-AuthenticodeSignature -LiteralPath $package
if ($signature.Status -ne 'Valid') { throw $signature.StatusMessage }
$report = [ordered]@{
    sha256 = $digest
    utc = [DateTime]::UtcNow.ToString('o')
    status = $signature.Status.ToString()
    signer_subject = $signature.SignerCertificate.Subject
    signer_thumbprint = $signature.SignerCertificate.Thumbprint
    timestamp_subject = $signature.TimeStamperCertificate.Subject
    scope = 'Windows trust verification of downloaded PE envelope; not inner PFS signatures or board acceptance'
}
$report | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $repo 'artifacts/firmware/recovery-authenticode.json') -Encoding utf8
$report | ConvertTo-Json
