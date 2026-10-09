# Experimental native drivers

Companion's product uses the Linux kernel's existing device drivers. The code
here belongs to the optional freestanding kernel lab and has not run on the Dell.
Direct product hardware control uses Linux interfaces and supported BIOS tools;
these native driver experiments are not required for the UI or control service.

`input/i8042.c` is the first experimental native device driver: bounded controller setup,
translated set-1 key decoding and polling input, tested with mock ports and real
QEMU keyboard injection. Physical Dell execution remains unverified.

`net/e1000.c` drives the emulated Intel 82540EM in the explicit VM network
fixture, using bounded polling and owned DMA rings. Native packet/authenticated
reboot tests pass; this is not a driver for the Dell's Realtek USB adapter.

`usb/xhci.c` owns the QEMU xHCI rings and reads device/configuration descriptors
at full/high/SuperSpeed through native EP0 transfers. Host failure fixtures and
four inspection VM cases pass. It halts and releases owned DMA after inspection;
the USB network profile retains it for native bulk IN/OUT. It has not run on
the physical Intel controller.
See [native USB proof](../docs/native-usb-verification.md).

`net/usb_ecm.c` selects and validates the isolated CDC ECM adapter configuration
and MAC, then carries native Ethernet over bulk endpoints. Authenticated status,
reboot/reconnect and frame-boundary rejection pass. This is not the Dell Realtek
vendor protocol. Its SuperSpeed parser/setup now passes the Dell's captured ECM
configuration in host models; physical traffic remains untested. See
[USB network proof](../docs/native-usb-network-verification.md) and
[Dell ECM evidence](../docs/dell-usb-ecm-investigation.md).

GOP and firmware keyboard access in the original UEFI experiment are firmware
protocols. The native kernel uses the inherited framebuffer and its own input
driver after ExitBootServices. Physical Realtek Ethernet, storage and accelerated
graphics remain to be implemented.
