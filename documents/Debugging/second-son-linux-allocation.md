# Second Son Linux loading crash: investigation

## Captured evidence

CUSA00223, app 01.07, eboot hash `0x5dc0133075c46f0e`, fork revision
`d32194e1`, CachyOS, Ryzen 5700X, RX 9060 XT / RADV 26.2.4.
Both host C++ standard-library builds reportedly fail after the studio logo.

The fault at guest `0x80045e231` is a vector store to
`r13 + r15 - 0x4c`. The captured registers are `r13=0`, `r15=0x4c`:
the destination is address zero. Direct CLI launch reproduces it.

An earlier capture stops at `0x80045df5e`, immediately after the call at
`0x80045df59` to `0x800059d50`. It returns `rax=0` for a request of
`0x17b2b80` bytes (24,849,280 bytes / 23.70 MiB), with 16-byte alignment.
The caller stores that result as the buffer pointer and later writes through it.

The captured allocator rounds the size to a multiple of 16. For requests above
1024 bytes it calls `0x80005a360` to search a guest free list:

- `heap+8`: first free-block node.
- `node+0x28`: block address.
- `node+0x30`: block length.
- `node+0x40`: next free-block node.

The search considers alignment padding and returns false if no block fits.
The allocation wrapper then returns null. This path does not request additional
memory from the host. The captures do not contain the free list, so exhaustion,
fragmentation and incorrect heap initialization remain to be distinguished.
No assertion bypass, arbitrary host malloc substitution or game-code workaround
has been applied.

The log also reports missing PS4 system fonts and `libSceLibcInternal.sprx`;
some PS4 libc imports resolve to stubs. These are compatibility concerns, but the
captures do not establish them as the cause of this custom-heap failure. Installing
Linux libc++ does not supply PS4 firmware libraries.

## Local checks

The uploaded executable was reconstructed from two archive parts. It is an
unstripped x86-64 ELF with symbols, build ID
`531bcdbfa410c37e70d0608f3ce4e84a9e1f8040`.
Its unmodified `--help` cannot run on this cloud host: libm lacks required
`GLIBC_2.43` and `GLIBC_2.44` symbol versions. This says nothing about its ability
to run on CachyOS, where those dependencies are available.

The uploaded guest allocator instructions were separately executed in a local
isolated C harness with synthetic heap metadata and the required guest canary.
They returned null for an empty free list, one undersized block, and two fragmented
blocks whose combined size exceeds the request but neither individually fits.
This verifies the failure-path interpretation; it does not reproduce the game's
actual heap history or prove a gameplay fix.

## Next host capture

Run from the repository directory after updating. No rebuild is required for the
diagnostic script; it observes the existing binary and never changes guest data:

```sh
gdb -q -batch -x scripts/debug-second-son-linux.py \
    --args ./build/linux/app/shadps4 \
    --game "/absolute/path/to/game-folder" \
    > ~/shadps4-heap-diagnostic.txt 2>&1
```

The script waits until guest code is mapped, verifies the known call instruction,
and installs software breakpoints before and after this allocation. On a null
return it prints the requested size/alignment, heap free list before and after,
total free bytes, largest block, number of aligned fits, registers and thread
stacks. It detects cycles and bounds traversal; unsupported instruction bytes or
unreadable memory are reported. Offsets are specific to this captured executable.

The diagnostic was exercised under GDB against the captured allocator in the
synthetic harness, including verification that fragmentation is correctly reported
before the caller uses the null result. Its resulting host report is needed to
choose a fix rather than conceal the crash.
