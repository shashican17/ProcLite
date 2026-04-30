#pragma once
#include "process.h"
#include "system_stats.h"
#include <vector>
#include <string>
#include <unordered_map>
#include <functional>
#include <climits>
#include <csignal>

enum class SortBy { CPU, MEMORY, PID, NAME };

struct FilterOptions {
    std::string name_pattern;
    std::string user_filter;
    int         pid_min = 0;
    int         pid_max = INT_MAX;
};

class ProcessManager {
public:
    ProcessManager();

    void refresh();

    const std::vector<Process>& processes() const { return processes_; }
    SystemStats&                sys_stats()        { return sys_stats_; }

    void set_sort(SortBy s)         { sort_by_ = s; }
    SortBy get_sort() const         { return sort_by_; }

    void set_filter(const FilterOptions& f) { filter_ = f; }
    void clear_filter()                     { filter_ = FilterOptions{}; }

    std::vector<Process> filtered_view() const;

    bool kill_process(int pid, int sig = SIGTERM);
    bool renice_process(int pid, int priority);

    std::vector<int> children_of(int pid) const;

    long total_mem_kb() const { return sys_stats_.total_mem_kb; }

private:
    std::vector<Process>              processes_;
    std::unordered_map<int, Process>  prev_snapshot_;
    SystemStats                       sys_stats_;
    SortBy                            sort_by_ = SortBy::CPU;
    FilterOptions                     filter_;
    long                              prev_total_cpu_ = 0;
    long                              curr_total_cpu_ = 0;
};
