#pragma once
#include <vector>
#include <string>

struct CoreStat {
    std::string label;   // "cpu0", "cpu1", …
    float       usage_pct = 0.0f;
};

// Holds two successive /proc/stat samples and exposes per-core %
class SystemStats {
public:
    // Call once to prime the first snapshot
    void prime();

    // Call after ≥1 s to compute delta percentages
    void refresh();

    // Aggregate (all cores combined) usage %
    float total_cpu_pct() const { return total_pct_; }

    // Per-core breakdown
    const std::vector<CoreStat>& per_core() const { return cores_; }

    long total_mem_kb  = 0;
    long free_mem_kb   = 0;
    long used_mem_kb   = 0;

private:
    struct RawCoreStat {
        std::string label;
        long user=0, nice=0, system=0, idle=0,
             iowait=0, irq=0, softirq=0, steal=0;
        long total()  const { return user+nice+system+idle+iowait+irq+softirq+steal; }
        long active() const { return user+nice+system+irq+softirq+steal; }
    };

    std::vector<RawCoreStat> prev_, curr_;
    std::vector<CoreStat>    cores_;
    float                    total_pct_ = 0.0f;

    std::vector<RawCoreStat> read_raw();
    void parse_meminfo();
};
