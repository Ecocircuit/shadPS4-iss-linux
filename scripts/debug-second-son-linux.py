"""Run with GDB -x (not ordinary Python); diagnostic only, never changes guest data.

    gdb -q -batch -x scripts/debug-second-son-linux.py --args build/linux/app/shadps4 --game /path/to/game

Offsets are for the captured Second Son 01.07 executable. Instruction bytes are
checked before installing guest breakpoints. The game must be launched directly.
"""
import struct
import gdb

CALL = 0x80045DF59
RETURN = 0x80045DF5E
pending = {}
call_count = 0
installed = []


def reg(name):
    return int(gdb.parse_and_eval("$" + name)) & ((1 << 64) - 1)


def read64(address):
    return struct.unpack("<Q", bytes(gdb.selected_inferior().read_memory(address, 8)))[0]


def free_blocks(heap, size, alignment):
    """The captured allocator scans heap+8, then node+0x40 for free blocks."""
    lines = []
    seen = set()
    total = largest = fitting = 0
    try:
        node = read64(heap + 8)
        while node:
            if node in seen or len(seen) >= 512:
                lines.append("STOP: cyclic or unusually long free list")
                break
            seen.add(node)
            start, length = read64(node + 0x28), read64(node + 0x30)
            padding = (-start) & (alignment - 1)
            fits = length >= size + padding
            total += length
            largest = max(largest, length)
            fitting += fits
            lines.append(f"node={node:#x} start={start:#x} bytes={length:#x} fits={fits}")
            node = read64(node + 0x40)
    except (gdb.error, struct.error) as error:
        lines.append(f"STOP: cannot read free list: {error}")
    summary = f"blocks={len(seen)} total_free={total:#x} largest={largest:#x} fitting={fitting}"
    return summary + "\n" + "\n".join(lines)


class BeforeAllocation(gdb.Breakpoint):
    def stop(self):
        global call_count
        call_count += 1
        heap, size, alignment = reg("rdi"), reg("rsi"), max(16, reg("rdx"))
        if alignment & (alignment - 1):
            snapshot = f"Unexpected alignment: {alignment:#x}"
        else:
            # The guest rounds size to a minimum of 16 and a multiple of 16.
            snapshot = free_blocks(heap, max(16, (size + 15) & ~15), alignment)
        pending[gdb.selected_thread().global_num] = (heap, size, alignment, snapshot)
        return False


class AfterAllocation(gdb.Breakpoint):
    def stop(self):
        entry = pending.pop(gdb.selected_thread().global_num, None)
        if reg("rax") or entry is None:
            return False
        heap, size, alignment, snapshot = entry
        gdb.write(f"\n=== NULL ALLOCATION: heap={heap:#x} size={size:#x} "
                  f"({size / 1048576:.2f} MiB) alignment={alignment:#x} ===\n")
        gdb.write(f"Calls observed at this allocation site: {call_count}\n")
        gdb.write("FREE LIST BEFORE CALL:\n" + snapshot + "\n")
        gdb.write("FREE LIST AFTER CALL:\n" + free_blocks(heap, size, alignment) + "\n")
        gdb.execute("info registers")
        gdb.execute(f"x/32gx {heap:#x}")
        gdb.execute("x/48gx $rsp")
        gdb.execute("thread apply all bt 12")
        gdb.write("Diagnostic stop: the game has not yet used the null allocation.\n")
        return True


class GameLoaded(gdb.Breakpoint):
    def stop(self):
        actual = bytes(gdb.selected_inferior().read_memory(CALL, 5))
        if actual != bytes.fromhex("e8f2bdbfff"):
            gdb.write(f"Unsupported executable: allocation call bytes are {actual.hex()}. "
                      "No guest breakpoints installed.\n")
            return True
        installed.extend([BeforeAllocation(f"*{CALL:#x}", internal=True),
                          AfterAllocation(f"*{RETURN:#x}", internal=True)])
        self.enabled = False
        gdb.write("Second Son allocation diagnostics installed (no guest data changes).\n")
        return False


for command in ("set pagination off", "set confirm off", "set debuginfod enabled off",
                "handle SIGSEGV nostop noprint pass", "handle SIGBUS nostop noprint pass",
                "handle SIGILL nostop noprint pass"):
    gdb.execute(command)
installed.append(GameLoaded("Core::Linker::Execute", internal=True))
gdb.execute("run")
