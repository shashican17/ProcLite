#include "process_manager.h"
#include "parser.h"
#include "logger.h"
#include "utils.h"

#include <algorithm>
#include <csignal>
#include <sys/resource.h>
#include <unordered_set>
#include <regex>
#include <climits>

ProcessManager::ProcessManager() {
    // Prime the system stats so the first delta is meaningful
    sys_stats_.prime();
    prev_total_cpu_ = Parser::parse_total_cpu_time();

    // First pass: fill prev_snapshot_ without computing CPU (no prev)
    for (int pid : Parser::list_pids()) {
        auto opt = Parser::parse_stat(pid);
        if (!opt) continue;
        prev_snapshot_[pid] = *opt;
    }
}

void ProcessManager::refresh() {
    // 1. Snapshot total CPU time (denominator for per-process delta)
    curr_total_cpu_ = Parser::parse_total_cpu_time();
    long delta_total = curr_total_cpu_ - prev_total_cpu_;

    // 2. Refresh system-wide stats (per-core %, memory)
    sys_stats_.refresh();

    // 3. Walk /proc
    auto pids = Parser::list_pids();
    std::unordered_set<int> live_pids(pids.begin(), pids.end());

    processes_.clear();
    processes_.reserve(pids.size());

    long total_mem = sys_stats_.total_mem_kb > 0 ? sys_stats_.total_mem_kb : 1;

    for (int pid : pids) {
        // --- parse stat ---
        auto opt = Parser::parse_stat(pid);
        if (!opt) continue;          // process vanished
        Process proc = *opt;

        // --- compute CPU delta ---
        if (prev_snapshot_.count(pid)) {
            const CpuSnapshot& prev = prev_snapshot_[pid].prev_snap;
            const CpuSnapshot& curr = proc.prev_snap;

            long proc_delta = (curr.utime + curr.stime) -
                              (prev.utime  + prev.stime);

            if (delta_total > 0 && proc_delta >= 0) {
                proc.cpu_usage = 100.0f *
                    static_cast<float>(proc_delta) /
                    static_cast<float>(delta_total);
            }
            proc.has_prev = true;
        }

        // --- parse status (memory, user, threads) ---
        Parser::parse_status(pid, proc);

        // --- full cmdline ---
        proc.command = Parser::parse_cmdline(pid);
        if (proc.command.empty()) proc.command = proc.name;

        // --- mem % ---
        proc.mem_percent = 100.0f *
            static_cast<float>(proc.mem_rss_kb) /
            static_cast<float>(total_mem);

        // --- log CPU spikes > 80% ---
        if (proc.cpu_usage > 80.0f)
            Logger::instance().log_cpu_spike(pid, proc.name, proc.cpu_usage);

        processes_.push_back(proc);
        prev_snapshot_[pid] = proc;
    }

    // Remove stale entries from prev_snapshot_ (dead processes)
    for (auto it = prev_snapshot_.begin(); it != prev_snapshot_.end(); ) {
        if (!live_pids.count(it->first)) it = prev_snapshot_.erase(it);
        else ++it;
    }

    prev_total_cpu_ = curr_total_cpu_;
}

// ── Filtered + sorted view ───────────────────────────────────────────────────

std::vector<Process> ProcessManager::filtered_view() const {
    std::vector<Process> view;
    view.reserve(processes_.size());

    bool has_name   = !filter_.name_pattern.empty();
    bool has_user   = !filter_.user_filter.empty();

    for (const auto& p : processes_) {
        // PID range
        if (p.pid < filter_.pid_min || p.pid > filter_.pid_max) continue;

        // User filter
        if (has_user && p.user != filter_.user_filter) continue;

        // Name / command filter (case-insensitive substring)
        if (has_name) {
            bool match = Utils::icontains(p.name,    filter_.name_pattern) ||
                         Utils::icontains(p.command, filter_.name_pattern);
            if (!match) continue;
        }

        view.push_back(p);
    }

    // Sort
    switch (sort_by_) {
        case SortBy::CPU:
            std::sort(view.begin(), view.end(),
                [](const Process& a, const Process& b){ return a.cpu_usage > b.cpu_usage; });
            break;
        case SortBy::MEMORY:
            std::sort(view.begin(), view.end(),
                [](const Process& a, const Process& b){ return a.mem_rss_kb > b.mem_rss_kb; });
            break;
        case SortBy::PID:
            std::sort(view.begin(), view.end(),
                [](const Process& a, const Process& b){ return a.pid < b.pid; });
            break;
        case SortBy::NAME:
            std::sort(view.begin(), view.end(),
                [](const Process& a, const Process& b){ return a.name < b.name; });
            break;
    }

    return view;
}

// ── Process control ──────────────────────────────────────────────────────────

bool ProcessManager::kill_process(int pid, int sig) {
    // Find name for logging before we kill it
    std::string name = std::to_string(pid);
    for (const auto& p : processes_)
        if (p.pid == pid) { name = p.name; break; }

    if (::kill(static_cast<pid_t>(pid), sig) == 0) {
        Logger::instance().log_kill(pid, name, sig);
        return true;
    }
    return false;
}

bool ProcessManager::renice_process(int pid, int priority) {
    return setpriority(PRIO_PROCESS,
                       static_cast<id_t>(pid),
                       priority) == 0;
}

// ── Tree helpers ─────────────────────────────────────────────────────────────

std::vector<int> ProcessManager::children_of(int pid) const {
    std::vector<int> kids;
    for (const auto& p : processes_)
        if (p.ppid == pid && p.pid != pid)
            kids.push_back(p.pid);
    return kids;
}
