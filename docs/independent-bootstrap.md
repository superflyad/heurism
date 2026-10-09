# Can Companion start without the SSD?

No SSD-independent Companion startup is physically proven. The working root
connection and automatic NVRAM extension still begin with the SSD bootstrap.
Investigation on 2026-09-27 identified a real network-boot candidate in the
saved Dell firmware, but has not turned it into an autonomous management path.

## New evidence

Seven modules were extracted from the pinned BIOS snapshot using the previously
verified UEFIExtract revision. The original ROM SHA-256 is
`09bc04700d0047b2317f865eafffaf77e9de500d3746e1ca0da9344d4690c46c`.
The modules are BdsDxe, BiosConnectLauncher, DellBiosConnectNetwork,
DellBiosConnectAdvancedDownloadMgr, HttpBootDxe, DellAutoOsRecoveryDxe and
SpecialBootStubDxe. Extraction only wrote research files; it did not execute
the vendor modules or read/program live SPI flash.

The live BIOS exposes BIOSConnect=Enabled, SupportAssistOSRecovery=Enabled,
AutoOSRecoveryThreshold=2 and UefiNwStack=Enabled. The model's
[Dell service manual](https://www.dell.com/support/manuals/en-et/inspiron-15-7506-2-in-1-laptop/inspiron-7506-2n1-black-service-manual/system-setup-options?guid=guid-cb19996e-6cf5-47b9-be58-1a039da03b99&lang=en-us)
describes BIOSConnect as cloud recovery after OS/local recovery failures.
[Dell's recovery documentation](https://www.dell.com/support/kbdoc/en-us/000177771/using-biosconnect-to-recover-supportassist-os-recovery-partition)
describes downloading its service OS from Dell. That is a built-in recovery
feature, not proof that it accepts our NVRAM extension or provides our root SSH.

The separate HttpBootDxe contains HTTP/HTTPS scheme strings, a Boot URI
configuration form and EFI_LOAD_FILE_PROTOCOL GUID references. This is a
concrete candidate for downloading an owner-selected initial executable.
[Dell's HTTPs Boot guide](https://www.dell.com/support/manuals/en-us/bios-connect/https_ug/https-boot-and-admin-password%3Fguid=guid-febf1f2b-0ca0-4db6-8c4f-3d4a9a9b473c&lang=en-us)
documents manual URLs pointing to .efi files on supported systems.
[Dell Command Configure](https://www.dell.com/support/manuals/en-sg/command-configure/dcc_ug_5.x/biosconnect-profiles?guid=guid-3119f084-754f-48b6-bbe4-c49263d86e82&lang=en-us)
documents an HttpBootProfile URL/certificate interface. Neither general guide
establishes that this particular 7506 exposes those profile controls. Its
currently enumerated Linux firmware attributes do not include an HTTP URL or
HTTP profile setting.

## Actual Dell code tested offline

`tools/test-dell-http-uri.py` executes only the saved HttpBootDxe scheme-check
instructions at RVA `0x34dc` in Unicorn 2.1.4. Its module SHA-256 is
`d992982aaef66ab249a4811d1a619eac3166e0cc66aef6a75e88014869020788`.
No firmware API is mocked or called by this leaf function; no network is started.

| Scheme fixture | Result |
| --- | --- |
| HTTP URL to the local controller | EFI_SUCCESS |
| HTTPS URL to the local controller | EFI_SUCCESS |
| Uppercase HTTPS to an owner hostname | EFI_SUCCESS; scheme lowercased |
| FTP | EFI_INVALID_PARAMETER |
| file URI | EFI_INVALID_PARAMETER |
| nvram URI | EFI_INVALID_PARAMETER |
| Empty string | EFI_INVALID_PARAMETER |

All seven assertions pass. Scheme acceptance is not full URL validation,
network transfer, TLS trust, image acceptance or physical boot success.
In particular, it does not explain or remove the earlier physical
EFI_ACCESS_DENIED result for plaintext HTTP.

The saved [physical network inventory](preboot-network-testing.md) found
HTTP service binding, but its two LoadFile handles had PXE device paths.
HTTP service binding alone does not establish a ready HTTP boot provider.
The existence of an HTTP boot module and configuration strings similarly does
not establish automatic publication for this USB Ethernet adapter.

## Routes and remaining gates

| Route | What is established | Missing requirement |
| --- | --- | --- |
| NVRAM bytes directly dispatched by Dell | Stored payload and SSD-assisted execution | A pre-existing firmware bootstrap/provider for variable images; none identified |
| Dell BIOSConnect recovery | Firmware modules and enabled setting | Owner-image selection, authentication, automatic startup and management access |
| Standard HTTP/HTTPS boot | Actual firmware driver and accepted URL schemes | Provider publication, supported configuration, IP acquisition, TLS trust and safe handoff |
| Native PXE | VM diskless management worked | Physical DHCP/fallback failed; do not repeat native boot selection |
| Existing Companion USB | USB provisioning previously booted and obtained root access | A separately verified current recovery image; it remains external storage |

Network boot could remove the SSD as the initial executable source. It would
still rely on an available network and durable files on a controller/server.
An OS running in RAM after download would not make RAM persistent storage:
reboot would have to retrieve the durable image again. Remote access would
begin only after the downloaded management system started; it would not grant
control of every earlier BIOS state.

The next useful safe test is to inspect HTTP boot/HII provider availability
through a read-only application launched by the established SSD path. Then,
if configuration and trust are understood, a bounded HTTPS transfer could
return explicitly to the normal SSD management loader on failure. These are
separate gates from SSD-independent startup. Do not select a native network
BootNext or remove/disable SSD recovery to try to prove independence.

That [physical interface inspection is now complete](http-interface-inspection.md).
HTTP/TLS services were present; the standard HTTP boot driver binding,
configuration GUID and HTTP LoadFile path were not observed in this boot state.
The later [targeted initialization test](http-driver-physical-initialization.md)
passed on hardware. Controller attachment, HTTP transfer and automatic
SSD-independent startup remain unresolved gates.

The follow-up [driver dispatch investigation](http-driver-dispatch.md) found
EFI_DEP_SOR (schedule on request) in the saved Dell HttpBootDxe dependency
section, and matched its containing volume to an exposed physical FV handle.
Fourteen offline tests of the actual Supported functions establish the
DHCP/HTTP/device-path prerequisites and error propagation. No driver was
scheduled, loaded or attached on hardware by that investigation; independent
startup remains unproven.

No physical reboot, boot-variable update, firmware-setting update, certificate
enrollment, native network boot or SSD removal was performed in this survey.
Pinned root SSH and the current healthy boot remained available.

## Evidence and reproduction

`tools/research-independent-bootstrap.py` extracts/copies the seven pinned
modules and captures relevant BIOS settings. `tools/analyze-bootstrap-providers.py`
records string/protocol references. `tools/test-dell-http-uri.py` performs the
bounded machine-code tests using the existing research virtual environment.

Evidence is under ignored `artifacts/research/independent-bootstrap/`:
`module-survey.json`, module binaries/disassemblies/strings,
`provider-references.json`, `uri-scheme-tests.json` and
`live-settings-and-health.txt`. String/GUID searches are candidate discovery,
not an exhaustive analysis of every firmware route.
