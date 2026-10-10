"""Write host-side recovery secrets with a restricted Windows ACL."""
import csv
import io
import subprocess


def write_restricted(path, value):
    if path.exists():
        raise FileExistsError(path)
    identity = subprocess.run(["whoami", "/user", "/fo", "csv", "/nh"],
                              capture_output=True, text=True, check=True).stdout
    sid = next(csv.reader(io.StringIO(identity)))[1]
    path.touch(exist_ok=False)
    try:
        subprocess.run(["icacls", str(path), "/inheritance:r", "/grant:r",
                        "*" + sid + ":(F)", "*S-1-5-18:(F)",
                        "*S-1-5-32-544:(F)"], check=True, capture_output=True)
        path.write_bytes((value + "\n").encode("ascii"))
    except Exception:
        path.unlink(missing_ok=True)
        raise
