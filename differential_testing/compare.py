
import json
import subprocess
from pathlib import Path

from spike_runner import run_spike

ROOT = Path(__file__).resolve().parent.parent
TESTS_DIR = ROOT / "differential_testing" / "tests"

KITES_EXECUTABLE = ROOT / "build" / "linux-test" / "kites-cli"
VM_STATE_DIR = ROOT / "vm_state"

KITES_REGISTER_FILE = VM_STATE_DIR / "registers_dump.json"
KITES_MEMORY_FILE = VM_STATE_DIR / "memory_dump.json"

GROUPS = [
    "gp_registers",
    "vector_registers",
    "vector_control_status_registers",
    "memory",
]


def load_json(path):
    if not path.exists():
        raise RuntimeError(f"Expected dump file was not generated: {path}")

    with open(path) as file:
        return json.load(file)


def run_kites(assembly_file, memory_ranges):
    command = [
        str(KITES_EXECUTABLE),
        str(assembly_file),
        "--vm", "rvss",
    ]

    for memory_range in memory_ranges:
        command.extend([
            "--mem-dump",
            memory_range["address"],
            str(memory_range["rows"]),
        ])

    result = subprocess.run(
        command,
        cwd=ROOT,
        capture_output=True,
        text=True,
    )

    if result.returncode != 0:
        raise RuntimeError(
            f"KITES execution failed:\n"
            f"{result.stdout}\n{result.stderr}"
        )

    state = load_json(KITES_REGISTER_FILE)

    if memory_ranges:
        state["memory"] = load_json(KITES_MEMORY_FILE)
    else:
        state["memory"] = {}

    return state


def compare_group(name, kites_state, spike_state, ignored):
    kites_registers = kites_state.get(name, {})
    spike_registers = spike_state.get(name, {})

    mismatches = []

    all_registers = sorted(
        set(kites_registers) | set(spike_registers)
    )

    for register in all_registers:
        if register in ignored:
            continue

        kites_value = kites_registers.get(register)
        spike_value = spike_registers.get(register)

        if kites_value is None or spike_value is None:
            mismatches.append(
                (register, kites_value, spike_value)
            )
        elif str(kites_value).lower() != str(spike_value).lower():
            mismatches.append(
                (register, kites_value, spike_value)
            )

    if mismatches:
        print(f"\n  FAIL: {name} ({len(mismatches)} mismatches)")

        for register, kites_value, spike_value in mismatches:
            print(
                f"    {register}: "
                f"KITES={kites_value}, Spike={spike_value}"
            )
    else:
        print(f"  PASS: {name}")

    return mismatches


def run_test(test_dir):
    test_name = test_dir.name

    print(f"\n{'=' * 40}")
    print(f"Running {test_name}")
    print("=" * 40)

    required_files = [
        test_dir / "kites.s",
        test_dir / "spike.s",
        test_dir / "config.json",
    ]

    for file in required_files:
        if not file.exists():
            raise RuntimeError(f"Missing required file: {file.name}")

    config = load_json(test_dir / "config.json")

    memory_ranges = config.get("memory_ranges", [])

    ignored_registers = {
        group: set(registers)
        for group, registers in config.get(
            "ignored_registers", {}
        ).items()
    }

    kites_state = run_kites(
        test_dir / "kites.s",
        memory_ranges,
    )

    spike_state = run_spike(
        test_dir / "spike.s",
        memory_ranges,
    )

    total_mismatches = 0

    for group in GROUPS:
        mismatches = compare_group(
            group,
            kites_state,
            spike_state,
            ignored_registers.get(group, set()),
        )
        total_mismatches += len(mismatches)

    if total_mismatches == 0:
        print(f"\n{test_name}: PASS")
        return True

    print(
        f"\n{test_name}: FAIL "
        f"({total_mismatches} mismatches)"
    )
    return False


def main():
    test_dirs = sorted(
        path for path in TESTS_DIR.iterdir()
        if path.is_dir()
    )

    if not test_dirs:
        print("No test directories found.")
        return

    results = {}

    for test_dir in test_dirs:
        try:
            results[test_dir.name] = run_test(test_dir)
        except Exception as error:
            print(f"\n{test_dir.name}: ERROR")
            print(f"  {error}")
            results[test_dir.name] = False

    passed = sum(results.values())
    total = len(results)
    failed = total - passed

    print("\n" + "=" * 40)
    print("TEST SUMMARY")
    print("=" * 40)

    for test_name, success in results.items():
        status = "PASS" if success else "FAIL"
        print(f"{test_name}: {status}")

    print(f"\n{passed}/{total} tests passed")

    if failed == 0:
        print("ALL DIFFERENTIAL TESTS PASSED")
    else:
        print(f"{failed} test(s) failed")


if __name__ == "__main__":
    main()
