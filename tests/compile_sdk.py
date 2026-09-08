"""Compile this wrapper against an existing IDF build's real headers, without PIO."""
import argparse
import json
from pathlib import Path
import shlex
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("database", type=Path, help="existing C6 IDF 5.5+ compile_commands.json")
parser.add_argument("output", type=Path, help="output directory under debug/")
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
output = args.output.resolve()
if "debug" not in [part.lower() for part in output.parts]:
    parser.error("output must be under a debug directory")
output.mkdir(parents=True, exist_ok=True)
entries = json.loads(args.database.read_text())
entry = next(e for e in entries if e["file"].replace("\\", "/").endswith("/ESP32-TWAI-CAN.cpp"))
command = shlex.split(entry["command"], posix=False)
command = [(a[1:-1] if a.startswith('"') and a.endswith('"') else a).replace('\\"', '"') for a in command]
# Discard dependency side effects targeting the original build directory.
base = []
skip = False
for arg in command:
    if skip:
        skip = False
        continue
    if arg in ("-MF", "-MT", "-MQ"):
        skip = True
    elif arg not in ("-MD", "-MMD", "-MP"):
        base.append(arg)
source = next(i for i, a in enumerate(base) if a.replace("\\", "/").endswith("/ESP32-TWAI-CAN.cpp"))
base[source] = str(root / "src/ESP32-TWAI-CAN.cpp")
base.insert(1, "-I" + str(root / "src"))
driver = next(a for a in base if a.endswith("/components/esp_driver_twai/include"))
for branch in ("new", "legacy"):
    command = base.copy()
    command[command.index("-o") + 1] = str(output / ("twai-sdk-" + branch + ".o"))
    if branch == "legacy":
        command.insert(1, "-I" + str(root / "tests/sdk_legacy"))
        command.insert(2, driver.replace("/esp_driver_twai/include", "/driver/twai/include"))
    subprocess.run(command, cwd=entry["directory"], check=True)
    command[command.index(str(root / "src/ESP32-TWAI-CAN.cpp"))] = str(root / "tests/driver_selection.cpp")
    command[command.index("-o") + 1] = str(output / ("twai-selection-" + branch + ".o"))
    command.insert(1, "-DTEST_TWAI_CONTROLLERS=" + ("1" if branch == "legacy" else "2"))
    subprocess.run(command, cwd=entry["directory"], check=True)
    print("PASS: actual wrapper + driver-selection assertions, real SDK headers, " + branch + " branch", flush=True)
