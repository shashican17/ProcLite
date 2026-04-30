# ProcLite — Lightweight Linux Process Manager

A `top`-like interactive TUI written in **pure C++17** with zero external
dependencies. Reads `/proc` directly, computes two-sample CPU deltas, and
renders a colour terminal UI using raw ANSI escape codes.

```
 ProcLite   Lightweight Linux Process Manager              [Sort: CPU]
CPU      [||||||||||||||||...................................] 28.4%
MEM 1.2G/15.6G [||..........................................]  7.9%

 PID    USER       STATE      CPU%   MEM%   THR  COMMAND
  4821  alice      Running    24.1   2.3      4  ./my_app --config prod.yml
  1042  root       Sleeping    3.2   0.1      1  /usr/sbin/sshd -D
   987  bob        Sleeping    0.8   0.4      6  /usr/bin/python3 server.py
   312  root       Zombie      0.0   0.0      1  [defunct]

 [q]Quit [k]Kill [r]Renice [/]Search [F]Filter [s]Sort [t]Tree [c]Cores [j/u]Scroll
```

---

## Table of Contents

1. [Project Structure](#project-structure)
2. [Architecture & Design](#architecture--design)
3. [System Calls & APIs Used](#system-calls--apis-used)
4. [Prerequisites](#prerequisites)
5. [Build Instructions](#build-instructions)
6. [Running ProcLite](#running-proclite)
7. [Interactive Keys](#interactive-keys)
8. [CLI Options](#cli-options)
9. [Testing Guide](#testing-guide)
10. [Edge Cases Handled](#edge-cases-handled)
11. [Logging](#logging)
12. [Extending the Project](#extending-the-project)

---

## Project Structure

```
ProcLite/
├── CMakeLists.txt          # CMake build definition
├── Makefile                # Convenience wrapper around CMake
├── README.md
├── include/
│   ├── process.h           # Process data model + CpuSnapshot
│   ├── parser.h            # /proc filesystem parsing declarations
│   ├── system_stats.h      # Per-core CPU & memory aggregates
│   ├── process_manager.h   # Orchestration: refresh / sort / filter / kill
│   ├── ui.h                # TUI rendering + raw-mode input
│   ├── logger.h            # Append-only event log
│   └── utils.h             # trim / split / icontains / human_size
└── src/
    ├── main.cpp            # Entry point, CLI parsing, signal handler
    ├── process.cpp         # Process::expand_state()
    ├── parser.cpp          # All /proc/* parsing logic
    ├── system_stats.cpp    # Two-sample per-core CPU deltas
    ├── process_manager.cpp # refresh(), filtered_view(), kill, renice
    ├── ui.cpp              # ANSI TUI, raw termios, interactive prompts
    ├── logger.cpp          # Singleton Logger
    └── utils.cpp           # String utilities
```

---

## Architecture & Design

```
main()
  │
  ├─► ProcessManager          (owns all process state)
  │     ├─► Parser::list_pids()          → enumerate /proc
  │     ├─► Parser::parse_stat()         → pid, ppid, name, state, CpuSnapshot
  │     ├─► Parser::parse_status()       → user (uid→name), VmRSS, threads
  │     ├─► Parser::parse_cmdline()      → full argv
  │     ├─► Parser::parse_total_cpu_time() → /proc/stat aggregate
  │     └─► SystemStats::refresh()       → per-core %, mem used/free
  │
  └─► UI                      (owns terminal I/O)
        ├─► enable_raw_mode() → termios: ECHO|ICANON|ISIG off, VMIN=0
        ├─► select() on stdin → non-blocking 1.5 s timeout
        ├─► draw_header()     → CPU/mem ANSI bar charts
        ├─► draw_process_table() → flat or tree view
        └─► draw_footer()     → keybinding reference + status message
```

### CPU Usage Algorithm

```
// Two-sample delta — same approach as top(1)

Sample 1  (at t=0):
  proc_time_1  = utime + stime          (from /proc/[pid]/stat)
  total_cpu_1  = sum(all fields)        (from /proc/stat, first line)

// sleep ~1.5 s

Sample 2  (at t=1):
  proc_time_2  = utime + stime
  total_cpu_2  = sum(all fields)

CPU% = (proc_time_2 - proc_time_1) / (total_cpu_2 - total_cpu_1) * 100
```

Division-by-zero is guarded: if `delta_total <= 0`, `cpu_usage` stays 0.

### Memory Calculation

```
VmRSS   ← /proc/[pid]/status   (resident set size, kB)
MemTotal ← /proc/meminfo        (total physical RAM, kB)

MEM% = VmRSS / MemTotal * 100
```

---

## System Calls & APIs Used

| Category        | Call / File                        | Purpose                          |
|-----------------|------------------------------------|----------------------------------|
| `/proc` parsing | `open()`, `read()`, `close()`      | Raw file I/O on procfs           |
| Dir enumeration | `opendir()`, `readdir()`           | Walk `/proc` for PID entries     |
| Process control | `kill(pid, SIGTERM/SIGKILL)`       | Terminate processes              |
| Priority        | `setpriority(PRIO_PROCESS, …)`     | Renice a process                 |
| User lookup     | `getpwuid()`                       | UID → username                   |
| Terminal size   | `ioctl(TIOCGWINSZ)`                | Responsive layout                |
| Terminal mode   | `tcgetattr()` / `tcsetattr()`      | Raw non-blocking input           |
| I/O multiplexing| `select()`                         | 1.5 s refresh with key interrupt |
| Signal handling | `sigaction(SIGINT, SIGTERM)`       | Clean terminal restore on exit   |

---

## Prerequisites

```bash
# Debian / Ubuntu
sudo apt update
sudo apt install -y build-essential cmake

# Fedora / RHEL
sudo dnf install -y gcc-c++ cmake

# Arch
sudo pacman -S base-devel cmake
```

Requires:
- **GCC ≥ 9** or **Clang ≥ 10** (C++17)
- **CMake ≥ 3.16**
- **Linux kernel** (needs `/proc` — not macOS/Windows)

---

## Build Instructions

### Release build (optimised, recommended)

```bash
git clone https://github.com/you/ProcLite.git
cd ProcLite
make          # equivalent to: cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build
```

Binary lands at `build/proclite`.

### Debug build (AddressSanitizer + UBSan enabled)

```bash
make debug
# binary at build_debug/proclite
```

### Manual CMake (if you don't want the Makefile wrapper)

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel $(nproc)
./build/proclite
```

### Install system-wide

```bash
sudo make install          # installs to /usr/local/bin/proclite
```

### Package (tarball + .deb)

```bash
cd build && cpack
# produces: proclite-1.0.0-Linux.tar.gz  and  proclite-1.0.0-Linux.deb
sudo dpkg -i proclite-1.0.0-Linux.deb
```

---

## Running ProcLite

```bash
# Default: sort by CPU, no filter
./build/proclite

# Start sorted by memory
./build/proclite --sort mem

# Show only processes matching "python"
./build/proclite --filter python

# Show only root's processes, start in tree view
./build/proclite --user root --tree

# Show per-core CPU bars immediately
./build/proclite --cores

# Combine flags
./build/proclite --sort mem --user alice --cores
```

---

## Interactive Keys

| Key      | Action                                                      |
|----------|-------------------------------------------------------------|
| `q`      | Quit                                                        |
| `k`      | Kill prompt → enter PID for SIGTERM, or `f <pid>` for SIGKILL |
| `r`      | Renice prompt → enter `<pid> <priority>` (−20 … 19)        |
| `/`      | Search by name or command substring (blank to clear)        |
| `F`      | Advanced filter: `user=alice pid=100-9999`                  |
| `s`      | Cycle sort: CPU → Memory → PID → Name → CPU …               |
| `t`      | Toggle process tree view (uses PPID from `/proc/[pid]/stat`)|
| `c`      | Toggle per-core CPU bars                                    |
| `j`      | Scroll process table down                                   |
| `u`      | Scroll process table up                                     |

---

## CLI Options

```
--sort cpu|mem|pid|name    Initial sort column (default: cpu)
--filter <pattern>         Pre-set name/command substring filter
--user <username>          Show only processes owned by this user
--cores                    Show per-core CPU bars on startup
--tree                     Start in process-tree view
-h, --help                 Print help and exit
```

---

## Testing Guide

### 1. Smoke test (automated)

```bash
make test
# Launches proclite, lets it run for 3 s, exits cleanly
```

### 2. CPU spike detection

```bash
# Terminal A – generate load
stress-ng --cpu 2 --timeout 30   # or: while true; do :; done &

# Terminal B – watch proclite
./build/proclite --sort cpu
# The stress-ng / shell process should appear at the top with CPU% > 0
# Check /tmp/proclite.log — spikes above 80% are logged automatically
```

### 3. Kill a process

```bash
# Terminal A
sleep 9999 &
echo "PID: $!"

# Terminal B – inside proclite
# Press k, enter the PID shown above, press Enter
# Verify in Terminal A that the process is gone
```

### 4. Force kill (SIGKILL)

```bash
# Inside the kill prompt, type:  f <pid>
# This sends SIGKILL instead of SIGTERM
```

### 5. Renice

```bash
# Start a background process
sleep 9999 &
BGPID=$!

# In proclite: press r, enter "<BGPID> 10"
# Verify:
cat /proc/$BGPID/stat | awk '{print "nice:", $19}'
```

### 6. Zombie process test

```bash
# Compile and run a zombie generator
cat > /tmp/zombie.c << 'C'
#include <stdio.h>
#include <unistd.h>
int main() {
    if (fork() == 0) _exit(0);   // child exits immediately → zombie
    printf("zombie PID created, parent sleeping\n");
    sleep(60);
}
C
gcc -o /tmp/zombie /tmp/zombie.c && /tmp/zombie &

# In proclite: the child shows as Zombie in red
```

### 7. Filter & search

```bash
# Search for bash processes
# Press /  → type "bash" → Enter
# Only bash-matching processes should appear

# Advanced filter by PID range
# Press F  → type "pid=1-100" → Enter
```

### 8. Tree view

```bash
./build/proclite --tree
# Press t to toggle; you should see parent→child relationships
# systemd (or init) at root with child processes indented beneath
```

### 9. Per-core CPU

```bash
./build/proclite --cores
# Press c to toggle
# Each cpu0, cpu1, … line shows individual core utilisation
```

### 10. Permission-denied handling

```bash
# Run as a normal user (not root)
./build/proclite --user root
# ProcLite should not crash — it skips unreadable /proc entries silently
# and shows "permission denied?" in the status bar when kill fails
```

### 11. High process-count stress

```bash
# Spawn 200 background sleeps
for i in $(seq 1 200); do sleep 9999 & done

# Run proclite — scroll with j/u, ensure no crash or corruption
./build/proclite
# Cleanup:
kill $(jobs -p)
```

### 12. Log verification

```bash
cat /tmp/proclite.log
# Expected format:
# [2025-04-30 14:22:01] KILL  pid=1234 name=sleep signal=SIGTERM
# [2025-04-30 14:22:45] SPIKE pid=5678 name=stress cpu=98.3%
```

---

## Edge Cases Handled

| Scenario                         | How it's handled                                         |
|----------------------------------|----------------------------------------------------------|
| Process exits mid-read           | `parse_stat()` returns `std::nullopt`; skipped silently  |
| Permission denied on `/proc/[pid]`| `ifstream::is_open()` fails; process skipped             |
| Zombie process                   | Parsed normally; shown in red; no CPU/mem misattribution |
| Division by zero in CPU calc     | `delta_total <= 0` guard → `cpu_usage = 0.0f`            |
| Huge `/proc` (thousands of PIDs) | `prev_snapshot_` cleaned of dead PIDs each refresh cycle |
| Terminal resize                  | `ioctl(TIOCGWINSZ)` called every draw frame              |
| Ctrl-C / SIGTERM                 | `sigaction` handler restores termios + shows cursor      |
| `comm` field with spaces/parens  | Parsed using `rfind(')')` not simple tokenisation        |
| UID without `/etc/passwd` entry  | Falls back to printing raw UID as string                 |

---

## Logging

Events are appended to `/tmp/proclite.log`:

```
[2025-04-30 14:10:01] KILL  pid=4512 name=firefox signal=SIGTERM
[2025-04-30 14:10:05] KILL  pid=4512 name=firefox signal=SIGKILL
[2025-04-30 14:11:43] SPIKE pid=9021 name=cc1     cpu=97.2%
```

CPU spikes are logged when a single process exceeds **80% CPU** in any sample.
Change the threshold in `process_manager.cpp`:

```cpp
if (proc.cpu_usage > 80.0f)   // ← adjust this
    Logger::instance().log_cpu_spike(…);
```

---

## Extending the Project

### Add regex filtering
Replace the `icontains` call in `ProcessManager::filtered_view()` with
`std::regex_search` — the `<regex>` header is already in the standard library.

### Export to JSON/CSV
Add a `--once` flag: run one refresh cycle, dump `filtered_view()` as JSON
to stdout, exit. Useful for scripting.

### Network I/O per process
Parse `/proc/[pid]/net/dev` or use `ss -tp` output mapped to PIDs.

### GPU utilisation
Query `nvidia-smi --query-compute-apps` or NVML API and cross-reference PIDs.

### Unit tests
The `Parser::` and `Utils::` namespaces are pure functions with no global
state — trivially testable with Catch2 or GoogleTest:

```bash
# Add to CMakeLists.txt
find_package(GTest REQUIRED)
add_executable(tests tests/parser_test.cpp src/parser.cpp src/utils.cpp)
target_link_libraries(tests GTest::gtest_main)
```
