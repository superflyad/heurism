#!/bin/sh
# Compatibility entry point for isolated VM Files and Editor interaction gates.
set -eu
directory=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
app=${1:-/var/lib/companion/native-stage/heurism-app}
sh "$directory/native_files_vm.sh" "$app"
sh "$directory/native_editor_vm.sh" "$app"
