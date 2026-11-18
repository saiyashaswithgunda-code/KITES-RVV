
import json
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent
KITES = ROOT / "build" / "headless" / "kites-cli.exe"
DUMP = ROOT / "vm_state" / "registers_dump.json"

def run_test(program, expected):
    print(f"\nRunning {program}...")

    result = subprocess.run(
        [str(KITES), str(ROOT / "examples" / program), "--vm", "rvss"],
        cwd=ROOT,
        capture_output=True,
        text=True
    )

    if result.returncode != 0:
        print("FAIL: KITES execution failed")
        print(result.stdout)
        print(result.stderr)
        return False

    try:
        with open(DUMP, "r") as file:
            state = json.load(file)
    except (OSError, json.JSONDecodeError) as error:
        print(f"FAIL: Could not read register dump: {error}")
        return False

    passed = True

    for section, registers in expected.items():
        for register, expected_value in registers.items():
            actual_value = state[section].get(register)

            if actual_value != expected_value:
                print(
                    f"FAIL: {section}.{register}: "
                    f"expected {expected_value}, got {actual_value}"
                )
                passed = False

    if passed:
        print(f"PASS: {program}")

    return passed


tests = {
    "vector_basic.s": {
        "gp_registers": {
            "x5": "0x0000000000000010"
        },
        "vector_registers": {
            "v1": "0x00000004000000030000000200000001",
            "v2": "0x00000008000000060000000400000002",
            "v3": "0x00000012000000100000000e0000000c"
        },
        "vector_control_status_registers": {
            "vstart": "0x0000000000000000",
            "vl": "0x0000000000000010",
            "vtype": "0x00000000000000c0",
            "vlenb": "0x0000000000000010"
        }
    }
}

passed = 0

for program, expected in tests.items():
    if run_test(program, expected):
        passed += 1

total = len(tests)

print("\n========== RVV TEST SUMMARY ==========")
print(f"Passed: {passed}/{total}")
print(f"Failed: {total - passed}/{total}")

sys.exit(0 if passed == total else 1)
