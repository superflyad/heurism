# Physical HTTP boot interface inspection

On 2026-09-27, an SSD-launched inventory application inspected the Dell's
standard HTTP/TLS, LoadFile, driver-binding and HII interfaces before the
management OS started. It completed and trusted root SSH returned without
physical intervention. SSD-independent boot remains unproven.

## Findings

| Interface or metadata | Physical result |
| --- | --- |
| HTTP service binding | One provider |
| TLS service binding | One provider |
| Existing HTTP child protocol | None |
| Existing TLS configuration child protocol | None |
| LoadFile | Two providers; both also expose PXE, with IPv4/IPv6 paths |
| LoadFile2 | None |
| URI-bearing LoadFile path | None |
| Driver bindings inspected | 65 image paths; no HttpBootDxe FFS GUID match |
| HII configuration access | 35 handles |
| HII package lists exported | 48; none skipped in the final pass |
| Standard HTTP boot HII package/form GUID | Not observed |
| HTTP_BOOT_CONFIG_IFR_NVDATA varstore metadata | Not observed |

The saved Dell HttpBootDxe contains its standard HII GUID
`4d20583a-7765-4e7a-8a67-dcde74ee3ec5`. The final physical report did not contain
that package/form GUID, the driver image GUID
`ecebcb00-d9c8-11e4-af3d-8cdcd426c973`, or its varstore name. One other package
contained the word HTTP; a keyword alone does not identify an HTTP boot form.

HTTP and TLS service bindings can create child interfaces, so the absence of
existing children is not evidence that those engines are unusable. Conversely,
their presence does not establish an HTTP boot provider. No child was created
and no driver was connected or started by this inspection.

This narrows the next gate: determine how Dell dispatches or enables the
existing HttpBootDxe, and whether it can expose a usable provider/configuration
interface under a bounded SSD-assisted experiment. A positive result would still
need IP acquisition, trusted HTTPS transfer, image execution and a separate
startup/fallback test before any SSD-independent claim.

## Probe and bounds

`boot/http_inspect_probe.c` uses standard LocateHandleBuffer, HandleProtocol,
LocateProtocol and HII ListPackageLists/GetPackageListHandle/ExportPackageLists.
It never calls LoadFile, HTTP request/configuration, driver Supported/Start/Stop,
ConnectController, HII ExtractConfig/RouteConfig/Callback, or firmware-variable
write services. The common framework writes a report on the ESP and attempts to
start the existing management loader. HII exports may notify their providers to
refresh package metadata; no BIOS configuration write method is invoked.

Only GUIDs, package lengths, formset/varstore metadata, keyword flags and device
path node summaries are saved. Raw configuration values, raw HII strings,
certificates and URI text are not saved. Driver/interface enumeration has fixed
handle limits. HII exports are bounded to 1 MiB per package list; packages and
IFR records have bounded loops and length checks. The report buffer is 128 KiB.

Host checks validate positive metadata fixtures, missing interfaces,
handle/export limits, invalid package/IFR/device-path lengths and the common
SSD handoff. A regression case covers successful exports that leave BufferSize
unchanged; the package's own declared length must be used instead.

## Physical runs and management

Three runs were needed to finish the inspection:

1. Boot `1d07aeca-9bfc-4413-ac7c-9bb1ef310f65` completed interface/binding
   enumeration, but its initial metadata parser rejected exports whose returned
   BufferSize still equalled the supplied capacity. The report was preserved.
2. Boot `787ff90d-cbb9-435b-93cb-802d62ee12f4` used the corrected parser. Four
   packages exceeded the initial 256 KiB export bound and were explicitly skipped.
3. Boot `b8c1de82-8a89-42de-945f-17d82af25c04` used the 1 MiB bound; all 48
   package exports were summarized without a skipped export.

All reports reached HTTP_INSPECTION_COMPLETE and PROBE_COMPLETE. Root SSH and
companion-watch returned healthy on each run. BootCurrent after management
returned was 0000, the established SSD fallback, rather than the temporary 0004
probe. Thus the evidence establishes probe completion and automatic SSD
management return; it does not establish which inner chainload step led to
BootCurrent changing to 0000. No firmware error-screen recovery was exercised.

Dell appended automatic USB NIC entries behind the two SSD entries during each
reboot. Their observed paths were saved and the known BootOrder 0005,0000 was
restored. Temporary owner entry 0004 was removed after each run. DriverOrder
remains 0000,0001; BootNext is absent. The owner NVRAM payload and all verified
management/recovery loader hashes remain unchanged. No native PXE selection,
SSD removal, certificate enrollment or BIOS-setting update was performed.

Final probe SHA-256:
`c0f50ba4da99f616eb08c373e449cecb5504934aefb391eeb84b794f435e6eac`.
Evidence is under ignored `artifacts/firmware/`: `http-inspect-observation.txt`,
`http-inspect-analysis.json`, `http-inspect-probe-deployment.json`, saved v1/v2
reports/metadata, and the post-run boot-variable backup. Host results are under
`build/http-inspect-probe/`.

Build with `tools/build-firmware-probe.ps1 -HttpInspectProbe`. Stage with
`tools/run-firmware-probe.py --http-inspect-probe`. The separate
`tools/verify-http-inspect.py reboot` checks the staged entry, image hash,
management health and recovery files before selecting that SSD probe once.
After reconnection and healthy confirmation, `verify` downloads the report,
removes the validated temporary entry and restores known default order.
`tools/analyze-http-inspection.py` summarizes the saved evidence offline.

Protocol definitions used are the pinned EDK II
[HII database interface](https://github.com/tianocore/edk2/blob/80d1e5f8474565ab198847325d951560d423d50b/MdePkg/Include/Protocol/HiiDatabase.h),
[driver binding interface](https://github.com/tianocore/edk2/blob/80d1e5f8474565ab198847325d951560d423d50b/MdePkg/Include/Protocol/DriverBinding.h),
and [HTTP boot HII GUID](https://github.com/tianocore/edk2/blob/80d1e5f8474565ab198847325d951560d423d50b/NetworkPkg/Include/Guid/HttpBootConfigHii.h).
