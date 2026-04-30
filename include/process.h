#pragma once
#include <string>
#include <vector>

// Represents a single snapshot of a process's CPU accounting fields
// pulled from /proc/[pid]/stat
struct CpuSnapshot {
    long utime  = 0;   // user mode ticks
    long stime  = 0;   // kernel mode ticks
    long cutime = 0;   // waited-for children user ticks
    long cstime = 0;   // waited-for children kernel ticks
};

class Process {
public:
    // --- identity ---
    int         pid      = 0;
    int         ppid     = 0;      // parent PID (for tree view)
    std::string name;              // comm field from /proc/[pid]/stat
    std::string command;           // full cmdline from /proc/[pid]/cmdline
    std::string user;              // owner resolved via /proc/[pid]/status uid
    std::string state;             // R S D Z T etc. → expanded to human label

    // --- resource usage ---
    float       cpu_usage  = 0.0f; // computed % after two-sample delta
    long        mem_rss_kb = 0;    // VmRSS in kB from /proc/[pid]/status
    float       mem_percent= 0.0f; // mem_rss_kb / total_mem * 100
    int         priority   = 0;    // nice value
    int         threads    = 0;    // Threads field from /proc/[pid]/status

    // --- previous sample (for delta CPU calc) ---
    CpuSnapshot prev_snap;
    bool        has_prev   = false;

    // Expand single-char state code → readable string
    static std::string expand_state(char c);
};
