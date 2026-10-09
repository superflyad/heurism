#!/bin/sh
# Run only on the isolated CompanionDev VM with its host reset path available.
set -eu
test "$(cat /sys/class/dmi/id/sys_vendor)" = 'Microsoft Corporation'
test "$(cat /sys/class/dmi/id/product_name)" = 'Virtual Machine'
release=/opt/heurism/native/current
"$release/heurism-release" verify
"$release/heurism-release" health >/dev/null
"$release/heurismctl" power-check
"$release/heurismctl" power '{"operation":"reboot","confirm":true}'
