#!/usr/bin/env python3
"""Host tests for the actual protocol and coordinator code; never contacts Dexcom."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument("--arduinojson", type=Path, required=True, help="ArduinoJson 7 src directory (contains ArduinoJson.h)")
args = parser.parse_args()
if not (args.arduinojson / "ArduinoJson.h").is_file():
    parser.error("--arduinojson must point to the ArduinoJson src directory")
root = Path(__file__).resolve().parent
component = root.parent / "components/clock_cgm"
cxx = os.environ.get("CXX", "clang++")
with tempfile.TemporaryDirectory(prefix="clock-cgm-tests-") as folder:
    for name, sources, stubs in [
        ("style", [root / "glucose_style_test.cpp"], False),
        ("sugarsense", [root / "sugarsense_test.cpp", component / "sugarsense_client.cpp"], False),
        ("commands", [root / "control_commands_test.cpp"], False),
        ("additional", [root / "additional_control_test.cpp", root.parent / "components/additional_cgm/additional_cgm.cpp", component / "provider_clients.cpp", component / "sugarsense_client.cpp", component / "provider_http.cpp"], True),
        ("providers", [root / "provider_clients_test.cpp", component / "provider_clients.cpp"], False),
        ("settings", [root / "setup_settings_test.cpp", component / "provider_clients.cpp"], False),
        ("dexcom", [root / "dexcom_test.cpp", component / "dexcom_client.cpp"], False),
        ("control", [root / "cgm_control_test.cpp", component / "clock_cgm.cpp", component / "dexcom_client.cpp", component / "provider_http.cpp"], True),
    ]:
        output = Path(folder) / name
        command = [cxx, "-std=c++17", "-Wall", "-Wextra", "-Werror", "-fsanitize=address,undefined", "-g",
                   "-I", str(root / "stubs"), "-I", str(root.parent.parent), "-I", str(component), "-I", str(root.parent / "components/clock_setup"), "-I", str(args.arduinojson)]
        if stubs:
            command += ["-I", str(root / "stubs")]
        subprocess.run(command + [str(p) for p in sources] + ["-o", str(output)], check=True)
        subprocess.run([str(output)], check=True)
