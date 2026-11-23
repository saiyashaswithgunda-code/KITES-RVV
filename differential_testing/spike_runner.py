
import json
import re
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BUILD_DIR = ROOT / "differential_testing" / "build"
BUILD_DIR.mkdir(parents=True, exist_ok=True)


def run_command(command, **kwargs):
    result = subprocess.run(
        command,
        capture_output=True,
        text=True,
        **kwargs
    )

    if result.returncode != 0:
        raise RuntimeError(
            f"Command failed: {' '.join(map(str, command))}\n"
            f"{result.stdout}\n{result.stderr}"
        )

    return result


def parse_spike_output(output, memory_ranges=None):
    memory_ranges = memory_ranges or []

    state = {
        "gp_registers": {},
        "vector_registers": {},
        "vector_control_status_registers": {},
        "memory": {}
    }

    # GPRs: ABI names in Spike output
    abi_to_x = {
        "zero": 0, "ra": 1, "sp": 2, "gp": 3, "tp": 4,
        "t0": 5, "t1": 6, "t2": 7, "s0": 8, "s1": 9,
        "a0": 10, "a1": 11, "a2": 12, "a3": 13,
        "a4": 14, "a5": 15, "a6": 16, "a7": 17,
        "s2": 18, "s3": 19, "s4": 20, "s5": 21,
        "s6": 22, "s7": 23, "s8": 24, "s9": 25,
        "s10": 26, "s11": 27, "t3": 28, "t4": 29,
        "t5": 30, "t6": 31
    }

    for name, value in re.findall(
        r"([a-z][a-z0-9]*):\s*(0x[0-9a-fA-F]+)", output
    ):
        if name in abi_to_x:
            state["gp_registers"][f"x{abi_to_x[name]}"] = value.lower()

    # Vector registers: combine high and low 64-bit halves
    for reg, high, low in re.findall(
        r"v(\d+)\s*:\s*\[1\]:\s*(0x[0-9a-fA-F]+)"
        r"\s+\[0\]:\s*(0x[0-9a-fA-F]+)", output
    ):
        value = (int(high, 16) << 64) | int(low, 16)
        state["vector_registers"][f"v{reg}"] = f"0x{value:032x}"

    # CSR values are printed after the PC
    csr_names = ["vl", "vtype", "vstart", "vlenb"]

    csr_values = re.findall(
        r"(?m)^0x([0-9a-fA-F]+)\s*$", output
    )

    if len(csr_values) < 5:
        raise RuntimeError("Could not parse PC and vector CSRs")

    # Last five standalone values: PC, vl, vtype, vstart, vlenb
    for name, value in zip(csr_names, csr_values[-4:]):
        state["vector_control_status_registers"][name] = (
            f"0x{int(value, 16):016x}"
        )

    # Memory: one 64-bit value per requested 8-byte row
    total_rows = sum(memory_range["rows"] for memory_range in memory_ranges)

    if len(csr_values) < 5 + total_rows:
        raise RuntimeError(
            f"Could not parse requested memory values: "
            f"expected {total_rows}, found {max(0, len(csr_values) - 5)}"
        )

    # Memory values precede the final five values (PC and CSRs)
    memory_values = csr_values[-(5 + total_rows):-5] if total_rows else []

    index = 0
    for memory_range in memory_ranges:
        address = int(memory_range["address"], 16)

        for row in range(memory_range["rows"]):
            current_address = address + row * 8
            state["memory"][f"0x{current_address:016x}"] = (
                f"0x{int(memory_values[index], 16):016x}"
            )
            index += 1

    if len(state["gp_registers"]) != 32:
        raise RuntimeError("Could not parse all 32 GPRs")

    if len(state["vector_registers"]) != 32:
        raise RuntimeError("Could not parse all 32 vector registers")

    if len(state["vector_control_status_registers"]) != 4:
        raise RuntimeError("Could not parse all four vector CSRs")

    return state


def run_spike(assembly_file, memory_ranges=None):
    assembly_file = Path(assembly_file).resolve()
    memory_ranges = memory_ranges or []

    name = assembly_file.parent.name

    elf_file = BUILD_DIR / f"{name}.elf"
    command_file = BUILD_DIR / f"{name}_commands.txt"
    output_file = BUILD_DIR / f"{name}_output.txt"
    state_file = BUILD_DIR / f"{name}_state.json"

    run_command([
        "riscv64-elf-gcc",
        "-march=rv64gcv",
        "-mabi=lp64d",
        "-nostdlib",
        "-nostartfiles",
        "-Wl,-Ttext=0x80000000",
        "-Wl,-e,_start",
        str(assembly_file),
        "-o", str(elf_file)
    ])

    symbols = run_command([
        "riscv64-elf-nm", "-n", str(elf_file)
    ]).stdout

    match = re.search(
        r"^([0-9a-fA-F]+)\s+\w\s+test_done$",
        symbols,
        re.MULTILINE
    )

    if not match:
        raise RuntimeError("Could not find test_done symbol in ELF")

    stop_pc = int(match.group(1), 16)

    commands = [
        f"until pc 0 0x{stop_pc:x}"
    ]

    for memory_range in memory_ranges:
        address = int(memory_range["address"], 16)

        for row in range(memory_range["rows"]):
            commands.append(f"mem 0x{address + row * 8:x}")

    commands.extend([
        "reg 0",
        "vreg 0",
        "pc 0",
        "reg 0 vl",
        "reg 0 vtype",
        "reg 0 vstart",
        "reg 0 vlenb",
        "quit"
    ])

    command_file.write_text("\n".join(commands) + "\n")

    result = subprocess.run(
        [
            "spike", "-d",
            f"--debug-cmd={command_file}",
            "-m0x7ffff000:0x10000000",
            "--isa=rv64gcv",
            "--varch=vlen:128,elen:64",
            str(elf_file)
        ],
        capture_output=True,
        text=True
    )

    if result.returncode != 0:
        raise RuntimeError(
            f"Spike execution failed for {assembly_file}\n"
            f"{result.stdout}\n{result.stderr}"
        )

    output = result.stdout + result.stderr
    output_file.write_text(output)

    state = parse_spike_output(output, memory_ranges)
    state_file.write_text(json.dumps(state, indent=2))

    print(f"Spike state saved: {state_file}")
    return state


if __name__ == "__main__":
    run_spike(
        ROOT / "differential_testing" / "tests" / "dtest1" / "spike.s"
    )
