# Dell HTTP driver initialization investigation

The [physical initialization test](http-driver-physical-initialization.md) now
confirms successful LoadImage/StartImage and both IPv4/IPv6 binding
registrations on the Dell. Their removal succeeded and root management
returned. Controller attachment, HTTP transfer and independent startup remain
unproven. The earlier saved-code research below established the entry and
policy prerequisites for this SSD-assisted test.

## Broader saved-ROM search

`tools/research-http-provider.py` scans an extracted dump of the pinned saved
ROM. It inspected 4,805 body files totaling 35,887,965 bytes, with file-count,
size and aggregate bounds. Files larger than 4 MiB are skipped for matching;
the normal individual PE candidates are smaller than this bound. The search
does not read/program live flash or invoke vendor firmware code.

Only HttpBootDxe itself among matched PE images contains the exact HTTP boot
file GUID. This expands the earlier seven-module search but still does not
exclude computed/indirect scheduling or other representations of that GUID.
Generic firmware boot-manager loading may use device paths rather than a
hard-coded driver GUID. A automatic startup route has not been identified.

The vendor interface GUID `8f63ff6d-b7d4-474d-8c53-68258a224d58` occurs in
multiple consumers. In the extracted DellBoardPolicyDxe, its entry at RVA
`0x700` installs that interface using a structure at `0xc38`, whose first method
is the policy lookup at `0x894`. The lookup searches two linked lists by request
GUID and invokes a matching registered callback. If no match exists it returns
EFI_NOT_FOUND. The default constructor adds three callback records; none
matches the HTTP constructor request GUID
`a686d83e-e7e7-4c01-8335-6fccb8e7c024`.

These were initially saved-code findings. The later physical test matched the
live interface owner/lookup instructions and inspected both callback lists;
neither contained the HTTP request GUID at that point.

## Entry replay: four cases

`tools/test-dell-http-entry.py` maps the actual saved HttpBootDxe PE, applies its
DIR64 relocations, supplies a synthetic EFI system/services table and executes
its entry point at RVA `0x9a8` with an instruction/time budget. It also maps the
actual saved DellBoardPolicyDxe and reconstructs only its three default linked
callback records. The two HTTP constructor calls execute the real policy
lookup instructions at `0x894..0x9c7`; both return EFI_NOT_FOUND without calling
a policy callback. HttpBootDxe explicitly treats that status as nonfatal.

Other services are explicit fixtures. The tests do not execute those services
on hardware or assume all live implementations have the same effects. Unknown
service calls and policy callback instruction ranges fail closed. A packed
vendor cleanup method used on registration failure has an explicitly synthetic
return value; its live semantics remain outside this proof.

| Fixture | Entry result |
| --- | --- |
| Default-table policy lookup; both registrations succeed | EFI_SUCCESS |
| First registration fails | Failure propagates; cleanup fixture invoked |
| Second registration fails | First registration uninstalled; failure propagates |
| Policy interface absent | EFI_SUCCESS; no policy method invoked |

All four pass. The success path registers IPv4 and IPv6 bindings plus their
component-name protocols and sets its loaded-image unload callback. No
ConnectController, controller Start, network transfer, global DXE dispatch or
firmware-variable write is called in these replay traces. This is evidence for
these fixtures, not a guarantee for every possible live policy callback.

## Revisiting the previous HTTP rejection

The saved HttpDxe transport module is distinct from HttpBootDxe. Its GUID
`2366c20f-e15a-11e3-8bf1-e4115b28bc50` appeared twice in the physical binding
inventory, consistent with IPv4/IPv6 driver bindings. Its saved PE was copied
and hashed for offline testing.

`tools/test-dell-http-request-gates.py` executes two bounded instruction slices
without a network request. Four URI cases run at `0x35fb..0x36ce`. HTTP reaches
the subsequent processing stage with UseHttps=false; HTTPS and uppercase HTTPS
reach it with UseHttps=true. An existing TLS child is supplied as a fixture to
skip TLS creation. FTP also reaches that point with false: this is URI
classification, not full URL acceptance. This slice does not contain the
plaintext-denial gate seen in the current reference implementation.

Three other cases execute the real duplicate-token callback at
`0x2638..0x2656`: a fresh token/event passes, the same token fails with
EFI_ACCESS_DENIED, and a different token sharing its event fails with the same
status. The surrounding saved Request code calls this callback and returns
EFI_ACCESS_DENIED if it fails. All seven cases pass.

The [reference HTTP implementation](https://github.com/tianocore/edk2/blob/master/NetworkPkg/HttpDxe/HttpImpl.c)
has both token-duplication and configurable plaintext restrictions. Our saved
instruction findings show why the earlier physical status alone does not
identify which failure occurred. They do not prove HTTP is usable on hardware,
establish that the earlier request collided with a token, or remove TLS trust
requirements. The physical rejection remains unexplained pending a bounded
diagnostic of the actual call path and state.

## Physical inventory gate

The [read-only physical inspection](http-prerequisite-inspection.md) completed
on boot `dbb8c399-d20b-4ee7-8218-d0512098a1bd`. Live FV2 reads matched all three
saved sections, the Ethernet controller has HTTP/DHCP4/DHCP6 bindings, and the
live policy interface/method are owned by the expected provider at the saved
RVAs. Root access returned, the temporary entry was removed, and the normal
SSD boot/driver order restored. This gate performed inspection only; the later
[physical initialization test](http-driver-physical-initialization.md) passed.

The read-only SSD-launched probe checked:

1. Read the specific existing HTTP boot firmware file and dependency section,
   then verify their pinned contents and report authentication metadata.
2. Check DHCP/HTTP/device-path protocol presence on the same NIC handle.
3. Identify the live policy interface's provider and compare it to the saved
   provider; record only provider identity/metadata, not configuration values.

These checks require neither global Dispatch nor a network boot selection.
Only after they match should targeted driver loading/registration and then
controller attachment be evaluated as separate experiments. Normal SSD boot
and recovery remain the confirmed management path. These findings do not grant
an independent reset channel or control of a Dell firmware halt screen.

## Evidence and state

Research scripts are `tools/research-http-provider.py`,
`tools/test-dell-http-entry.py` and `tools/test-dell-http-request-gates.py`.
Run the replays with `artifacts/research-venv/Scripts/python.exe`. Raw survey,
copied candidates, disassemblies and replay JSONs are under ignored
`artifacts/research/independent-bootstrap/`.

| Module | SHA256 |
| --- | --- |
| HttpBootDxe | `d992982aaef66ab249a4811d1a619eac3166e0cc66aef6a75e88014869020788` |
| DellBoardPolicyDxe | `31cf5121722bbff0cc5096b7a87d628669a8700ee149787477c72b2d787226fa` |
| HttpDxe | `9c83888402e904cd0866fd5b8415a73c6c3ce66a629be4dde992d7c806d87831` |

Root management was rechecked on unchanged boot
`b8c1de82-8a89-42de-945f-17d82af25c04`. SSH and companion-watch are started;
BootOrder is `0005,0000`, DriverOrder `0000,0001`, and BootNext is absent. This
research performed no physical reboot, firmware configuration update, network
boot selection or flash programming. Remote writes expanded/copied only the
already saved ROM into research files.
