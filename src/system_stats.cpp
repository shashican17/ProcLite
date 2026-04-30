#include "system_stats.h"
#include <fstream>
#include <sstream>
#include <string>
#include <algorithm>

// ── helpers ──────────────────────────────────────────────────────────────────

std::vector<SystemStats::RawCoreStat> SystemStats::read_raw() {
    std::vector<RawCoreStat> stats;
    std::ifstream f("/proc/stat");
    if (!f.is_open()) return stats;

    std::string line;
    while (std::getline(f, line)) {
        if (line.rfind("cpu", 0) != 0) break;  // cpu lines are contiguous at top
        std::istringstream ss(line);
        RawCoreStat c;
        ss >> c.label >> c.user >> c.nice >> c.system >> c.idle
           >> c.iowait >> c.irq >> c.softirq >> c.steal;
        stats.push_back(c);
    }
    return stats;
}

void SystemStats::parse_meminfo() {
    std::ifstream f("/proc/meminfo");
    if (!f.is_open()) return;

    long mem_available = 0;
    std::string line;
    while (std::getline(f, line)) {
        std::istringstream ss(line);
        std::string key; long val;
        ss >> key >> val;
        if      (key == "MemTotal:")     total_mem_kb = val;
        else if (key == "MemAvailable:") mem_available = val;
    }
    free_mem_kb = mem_available;
    used_mem_kb = total_mem_kb - free_mem_kb;
}

// ── public API ───────────────────────────────────────────────────────────────

void SystemStats::prime() {
    prev_ = read_raw();
    parse_meminfo();
}

void SystemStats::refresh() {
    curr_ = read_raw();
    parse_meminfo();

    cores_.clear();
    total_pct_ = 0.0f;

    // Compute per-core usage from delta
    size_t n = std::min(prev_.size(), curr_.size());
    for (size_t i = 0; i < n; ++i) {
        long delta_total  = curr_[i].total()  - prev_[i].total();
        long delta_active = curr_[i].active() - prev_[i].active();
        float pct = 0.0f;
        if (delta_total > 0)
            pct = 100.0f * static_cast<float>(delta_active) /
                           static_cast<float>(delta_total);

        // "cpu" (aggregate) drives total_pct_; cpu0,cpu1,… go into cores_
        if (curr_[i].label == "cpu") {
            total_pct_ = pct;
        } else {
            CoreStat cs;
            cs.label     = curr_[i].label;
            cs.usage_pct = pct;
            cores_.push_back(cs);
        }
    }

    prev_ = curr_;
}
