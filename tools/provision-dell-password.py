"""Set a random local account password without changing pinned key-only SSH.

Host-side management tool. The recovery secret is stored in a Windows file with
an explicit owner/System/Administrators ACL before the remote account changes.
"""
import argparse
import json
from pathlib import Path
import secrets
import subprocess
from secret_acl import write_restricted


REPO = Path(__file__).resolve().parents[1]
IDENTITY = REPO / "artifacts/ssh"
TARGET = json.loads((REPO / "artifacts/targets/dell.json").read_text())
ADDRESS = TARGET["address"]
EXPECTED_BOOT = TARGET["boot_id"]


def ssh(command, *, input_text=None):
    args = ["ssh", "-i", str(IDENTITY / "companion_client_ed25519"),
            "-o", "BatchMode=yes", "-o", "ConnectTimeout=5",
            "-o", "StrictHostKeyChecking=yes", "-o", "HostKeyAlias=companion-dell",
            "-o", "UserKnownHostsFile=" + str(IDENTITY / "known_hosts"),
            "root@" + ADDRESS, command]
    return subprocess.run(args, input=input_text, text=True, capture_output=True,
                          timeout=20, check=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("account", choices=("root", "companion-ui"))
    args = parser.parse_args()
    account = args.account
    secret_file = IDENTITY / ("dell-root-console-recovery.txt" if account == "root"
                              else "dell-desktop-unlock.txt")
    if secret_file.exists():
        raise SystemExit("Recovery file already exists; refusing to replace the credential")
    preflight = ("test \"$(cat /sys/class/dmi/id/sys_vendor)\" = 'Dell Inc.' && "
                 "test \"$(cat /sys/class/dmi/id/product_name)\" = 'Inspiron 7506 2n1' && "
                 "cat /proc/sys/kernel/random/boot_id && "
                 "grep '^" + account + ":' /etc/shadow | cut -d: -f2 | wc -c && "
                 "grep -E '^(PermitRootLogin|PasswordAuthentication|KbdInteractiveAuthentication|PubkeyAuthentication)' /etc/ssh/sshd_config")
    before = ssh(preflight).stdout.splitlines()
    expected_length = "1" if account == "root" else "2"
    if (before[0] != EXPECTED_BOOT or before[1] != expected_length or
            "PermitRootLogin prohibit-password" not in before or
            "PasswordAuthentication no" not in before or
            "KbdInteractiveAuthentication no" not in before or
            "PubkeyAuthentication yes" not in before):
        raise SystemExit("Dell identity, boot, or SSH/account preflight changed")
    password = secrets.token_hex(16)
    try:
        write_restricted(secret_file, password)
        ssh("test \"$(cat /proc/sys/kernel/random/boot_id)\" = '" + EXPECTED_BOOT +
            "' && chpasswd", input_text=account + ":" + password + "\n")
        after = ssh(preflight).stdout.splitlines()
        if (after[0] != EXPECTED_BOOT or int(after[1]) <= 1):
            raise RuntimeError("Credential change could not be verified")
    except Exception:
        print("Check Dell account state and the retained recovery file:", secret_file)
        raise
    print("Dell", account, "now has a random password; pinned root SSH still works.")
    print("Recovery credential:", secret_file)


if __name__ == "__main__":
    main()
