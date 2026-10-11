"""Keep Dell password provisioning byte-exact across Windows and Linux."""
import importlib.util
from pathlib import Path
import sys
from unittest.mock import patch


TOOLS = Path(__file__).resolve().parents[1] / "tools"
sys.path.insert(0, str(TOOLS))
spec = importlib.util.spec_from_file_location(
    "provision_dell_password", TOOLS / "provision-dell-password.py")
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)

with patch.object(module.subprocess, "run") as run:
    module.ssh("chpasswd", input_text="companion-ui:example\n")
    assert run.call_args.kwargs["input"] == b"companion-ui:example\n"
    assert run.call_args.kwargs["text"] is False

with patch.object(module.subprocess, "run") as run:
    module.ssh("cat /proc/sys/kernel/random/boot_id")
    assert run.call_args.kwargs["input"] is None
    assert run.call_args.kwargs["text"] is True

print("Dell password input preserves LF bytes")
