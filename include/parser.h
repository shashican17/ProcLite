#pragma once
#include "process.h"
#include <string>
#include <vector>
#include <optional>

// All raw /proc parsing lives here.
// Every function returns std::optional so callers can handle
// "process vanished mid-read" without exceptions.

namespace Parser {

    // Parse /proc/[pid]/stat → fills pid, ppid, name, state, CpuSnapshot
    std::optional<Process> parse_stat(int pid);

    // Parse /proc/[pid]/status → fills user, mem_rss_kb, threads
    bool parse_status(int pid, Process& proc);

    // Parse /proc/[pid]/cmdline → fills command (argv joined by spaces)
    std::string parse_cmdline(int pid);

    // Parse /proc/stat → returns total CPU time (sum of all fields on 'cpu' line)
    long parse_total_cpu_time();

    // Parse /proc/meminfo → total physical RAM in kB
    long parse_total_memory_kb();

    // Enumerate all numeric entries under /proc → list of live PIDs
    std::vector<int> list_pids();

    // Resolve numeric UID → username via /etc/passwd
    std::string uid_to_username(int uid);
}
