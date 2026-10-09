#!/bin/sh
# Run only on the isolated CompanionDev VM with its host reset path available.
set -eu
test "$(cat /sys/class/dmi/id/sys_vendor)" = 'Microsoft Corporation'
test "$(cat /sys/class/dmi/id/product_name)" = 'Virtual Machine'
release=/opt/companion/native/current
"$release/companion-release" verify
"$release/companion-release" health >/dev/null
"$release/companionctl" power-check
"$release/companionctl" power '{"operation":"reboot","confirm":true}'
