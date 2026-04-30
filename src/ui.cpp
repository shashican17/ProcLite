#include "ui.h"
#include "utils.h"

#include <iostream>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <csignal>
#include <algorithm>
#include <map>
#include <unordered_set>
#include <functional>

// ── ANSI escape helpers ───────────────────────────────────────────────────────
namespace A {
    const char* RESET    = "\033[0m";
    const char* BOLD     = "\033[1m";
    const char* DIM      = "\033[2m";
    const char* CLEAR    = "\033[2J\033[H";
    const char* HIDE_CUR = "\033[?25l";
    const char* SHOW_CUR = "\033[?25h";

    std::string fg(int n)          { return "\033[38;5;" + std::to_string(n) + "m"; }
    std::string bg(int n)          { return "\033[48;5;" + std::to_string(n) + "m"; }
    std::string move(int r, int c) { return "\033[" + std::to_string(r) + ";" + std::to_string(c) + "H"; }

    const char* HEADER_BG  = "\033[48;5;234m";
    const char* ACCENT     = "\033[38;5;45m";
    const char* ACCENT2    = "\033[38;5;214m";
    const char* GREEN      = "\033[38;5;82m";
    const char* RED        = "\033[38;5;196m";
    const char* YELLOW     = "\033[38;5;226m";
    const char* COL_HEADER = "\033[38;5;250m\033[48;5;236m";
    const char* ROW_EVEN   = "\033[38;5;253m\033[48;5;232m";
    const char* ROW_ODD    = "\033[38;5;253m\033[48;5;233m";
    const char* ZOMBIE_CLR = "\033[38;5;196m";
    const char* FOOTER_BG  = "\033[48;5;235m\033[38;5;244m";
}

// ── Constructor / Destructor ──────────────────────────────────────────────────

UI::UI(ProcessManager& pm) : pm_(pm) {}

UI::~UI() {
    disable_raw_mode();
    std::cout << A::SHOW_CUR << A::RESET << "\n";
}

// ── Terminal setup ────────────────────────────────────────────────────────────

void UI::enable_raw_mode() {
    tcgetattr(STDIN_FILENO, &orig_termios_);
    struct termios raw = orig_termios_;
    raw.c_iflag &= ~(IXON | ICRNL);
    raw.c_lflag &= ~(ECHO | ICANON | ISIG);
    raw.c_cc[VMIN]  = 0;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    raw_mode_ = true;
    std::cout << A::HIDE_CUR;
}

void UI::disable_raw_mode() {
    if (raw_mode_) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios_);
        raw_mode_ = false;
    }
}

void UI::get_terminal_size() {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0) {
        term_rows_ = ws.ws_row;
        term_cols_ = ws.ws_col;
    }
}

char UI::read_key() {
    char c = 0;
    if (read(STDIN_FILENO, &c, 1) == 1) return c;
    return 0;
}

// ── Main loop ─────────────────────────────────────────────────────────────────

void UI::run() {
    enable_raw_mode();
    pm_.refresh();

    while (true) {
        get_terminal_size();
        draw();

        // Wait up to 1500 ms for a key
        struct timeval tv = { 1, 500000 };
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(STDIN_FILENO, &fds);
        select(STDIN_FILENO + 1, &fds, nullptr, nullptr, &tv);

        char key = read_key();
        if (key == 'q' || key == 'Q') break;

        switch (key) {
            case 'k': case 'K': prompt_kill();    break;
            case 'r': case 'R': prompt_renice();  break;
            case '/': case 'f': prompt_search();  break;
            case 'F':           prompt_filter();  break;
            case 's': case 'S': cycle_sort();     break;
            case 't': case 'T': show_tree_  = !show_tree_;  break;
            case 'c': case 'C': show_cores_ = !show_cores_; break;
            case 'j':           scroll_offset_++;            break;
            case 'u':           scroll_offset_ = std::max(0, scroll_offset_ - 1); break;
            default: break;
        }

        pm_.refresh();
        scroll_offset_ = std::max(0, scroll_offset_);
    }
}

// ── Drawing ───────────────────────────────────────────────────────────────────

void UI::clear_screen() {
    std::cout << A::CLEAR;
}

void UI::draw() {
    clear_screen();
    auto procs = pm_.filtered_view();
    draw_header();
    draw_process_table(procs);
    draw_footer();
    std::cout.flush();
}

void UI::draw_header() {
    auto& ss = pm_.sys_stats();
    get_terminal_size();

    // Title bar
    std::cout << A::HEADER_BG << A::BOLD << A::ACCENT;
    std::cout << " ProcLite ";
    std::cout << A::DIM << A::RESET << A::HEADER_BG << A::fg(244);
    std::cout << "  Lightweight Linux Process Manager";

    std::string sort_label;
    switch (pm_.get_sort()) {
        case SortBy::CPU:    sort_label = "[Sort: CPU]";  break;
        case SortBy::MEMORY: sort_label = "[Sort: MEM]";  break;
        case SortBy::PID:    sort_label = "[Sort: PID]";  break;
        case SortBy::NAME:   sort_label = "[Sort: NAME]"; break;
    }
    int pad = term_cols_ - 36 - (int)sort_label.size();
    if (pad > 0) std::cout << std::string(pad, ' ');
    std::cout << A::ACCENT2 << sort_label;
    std::cout << A::RESET << "\n";

    // CPU bar
    draw_cpu_bar(ss.total_cpu_pct(), term_cols_ - 20, "CPU");

    // Memory bar
    float mem_pct = ss.total_mem_kb > 0 ?
        100.0f * ss.used_mem_kb / ss.total_mem_kb : 0.0f;
    std::string mem_label = "MEM " + Utils::human_size(ss.used_mem_kb) +
                            "/" + Utils::human_size(ss.total_mem_kb);
    draw_cpu_bar(mem_pct, term_cols_ - 20, mem_label);

    // Per-core bars
    if (show_cores_) {
        for (const auto& core : ss.per_core())
            draw_cpu_bar(core.usage_pct, term_cols_ - 20, core.label);
    }

    std::cout << "\n";
}

void UI::draw_cpu_bar(float pct, int width, const std::string& label) {
    if (width < 10) width = 10;
    pct = Utils::clamp(pct, 0.0f, 100.0f);
    int filled = static_cast<int>(pct * width / 100.0f);

    const char* colour = (pct < 50.0f) ? A::GREEN :
                         (pct < 80.0f) ? A::YELLOW : A::RED;

    std::cout << A::BOLD << A::ACCENT << std::setw(8) << std::left << label
              << A::RESET << " [";
    std::cout << colour << std::string(filled, '|');
    std::cout << A::DIM  << std::string(width - filled, '.');
    std::cout << A::RESET << "] ";
    std::cout << A::BOLD << std::fixed << std::setprecision(1) << pct << "%"
              << A::RESET << "\n";
}

void UI::draw_process_table(const std::vector<Process>& procs) {
    constexpr int W_PID   = 7;
    constexpr int W_USER  = 10;
    constexpr int W_STATE = 10;
    constexpr int W_CPU   = 7;
    constexpr int W_MEM   = 7;
    constexpr int W_THR   = 4;

    int W_CMD = term_cols_ - W_PID - W_USER - W_STATE - W_CPU - W_MEM - W_THR - 8;
    if (W_CMD < 12) W_CMD = 12;

    // Header row
    std::cout << A::COL_HEADER << A::BOLD;
    std::cout << std::setw(W_PID)   << std::left << " PID"
              << std::setw(W_USER)  << "USER"
              << std::setw(W_STATE) << "STATE"
              << std::setw(W_CPU)   << "CPU%"
              << std::setw(W_MEM)   << "MEM%"
              << std::setw(W_THR)   << "THR"
              << "COMMAND"
              << A::RESET << "\n";

    int core_rows  = show_cores_ ? (int)pm_.sys_stats().per_core().size() : 0;
    int table_rows = term_rows_ - 7 - core_rows;
    if (table_rows < 1) table_rows = 1;

    if (show_tree_) {
        // Build PID lookup
        std::map<int, const Process*> pmap;
        for (const auto& p : procs) pmap[p.pid] = &p;

        std::unordered_set<int> visible_pids;
        for (const auto& p : procs) visible_pids.insert(p.pid);

        int row = 0;
        std::function<void(int, const std::string&, bool)> print_tree =
            [&](int pid, const std::string& prefix, bool last) {
                auto it = pmap.find(pid);
                if (it == pmap.end()) return;
                const Process& p = *it->second;

                if (row >= scroll_offset_ && row < scroll_offset_ + table_rows) {
                    std::string branch = prefix + (last ? "L-- " : "|-- ");
                    std::string name_str = branch + p.name;
                    bool is_zombie = (p.state == "Zombie");

                    std::cout << (is_zombie ? A::ZOMBIE_CLR :
                                 (row % 2 == 0 ? A::ROW_EVEN : A::ROW_ODD));
                    std::cout << std::setw(W_PID)  << std::left << p.pid
                              << std::setw(W_USER) << truncate(p.user, W_USER)
                              << std::setw(W_STATE)<< truncate(p.state, W_STATE)
                              << std::setw(W_CPU)  << std::fixed << std::setprecision(1) << p.cpu_usage
                              << std::setw(W_MEM)  << std::fixed << std::setprecision(1) << p.mem_percent
                              << std::setw(W_THR)  << p.threads
                              << truncate(name_str, W_CMD)
                              << A::RESET << "\n";
                }
                row++;

                // Print children
                std::vector<int> kids;
                for (const auto& kv : pmap)
                    if (kv.second->ppid == pid && kv.first != pid)
                        kids.push_back(kv.first);

                for (size_t i = 0; i < kids.size(); ++i)
                    print_tree(kids[i], prefix + (last ? "    " : "|   "),
                               i == kids.size() - 1);
            };

        for (const auto& p : procs)
            if (!visible_pids.count(p.ppid) || p.ppid == p.pid)
                print_tree(p.pid, "", true);

    } else {
        int count = 0;
        for (int i = scroll_offset_;
             i < (int)procs.size() && count < table_rows;
             ++i, ++count)
        {
            const Process& p = procs[i];
            bool is_zombie = (p.state == "Zombie");

            if (is_zombie)
                std::cout << A::ZOMBIE_CLR;
            else
                std::cout << (count % 2 == 0 ? A::ROW_EVEN : A::ROW_ODD);

            std::ostringstream cpu_ss;
            cpu_ss << std::fixed << std::setprecision(1) << p.cpu_usage;

            std::cout << std::setw(W_PID)   << std::left << p.pid
                      << std::setw(W_USER)  << truncate(p.user, W_USER)
                      << std::setw(W_STATE) << truncate(p.state, W_STATE)
                      << std::setw(W_CPU)   << cpu_ss.str()
                      << std::setw(W_MEM)   << std::fixed << std::setprecision(1) << p.mem_percent
                      << std::setw(W_THR)   << p.threads
                      << truncate(p.command, W_CMD)
                      << A::RESET << "\n";
        }
        // Fill empty rows
        for (int i = count; i < table_rows; ++i)
            std::cout << "\n";
    }
}

void UI::draw_footer() {
    std::cout << A::FOOTER_BG;
    std::string keys = " [q]Quit [k]Kill [r]Renice [/]Search [F]Filter [s]Sort [t]Tree [c]Cores [j/u]Scroll";
    std::cout << truncate(keys, term_cols_);
    if (!status_msg_.empty()) {
        std::cout << "\n" << A::ACCENT2 << " " << status_msg_;
        status_msg_.clear();
    }
    std::cout << A::RESET << "\n";
}

// ── Interactive prompts ────────────────────────────────────────────────────────

void UI::prompt_kill() {
    disable_raw_mode();
    std::cout << A::SHOW_CUR << "\n";
    std::cout << A::ACCENT2 << "Kill PID (or 'f <pid>' for SIGKILL): " << A::RESET;
    std::string input;
    std::getline(std::cin, input);
    input = Utils::trim(input);

    int sig = SIGTERM;
    if (input.size() > 2 && input[0] == 'f' && input[1] == ' ') {
        sig   = SIGKILL;
        input = input.substr(2);
    }

    try {
        int pid = std::stoi(input);
        bool ok = pm_.kill_process(pid, sig);
        status_msg_ = ok ?
            "Sent signal " + std::to_string(sig) + " to PID " + std::to_string(pid) :
            "Failed to kill PID " + std::to_string(pid) + " (permission denied?)";
    } catch (...) {
        status_msg_ = "Invalid PID.";
    }

    enable_raw_mode();
    std::cout << A::HIDE_CUR;
}

void UI::prompt_renice() {
    disable_raw_mode();
    std::cout << A::SHOW_CUR << "\n";
    std::cout << A::ACCENT2 << "Renice <pid> <priority (-20..19)>: " << A::RESET;
    std::string line;
    std::getline(std::cin, line);
    std::istringstream ss(line);
    int pid = 0, pri = 0;
    if (ss >> pid >> pri) {
        bool ok = pm_.renice_process(pid, pri);
        status_msg_ = ok ? "Reniced PID " + std::to_string(pid) :
                           "Renice failed (need root for negative values)";
    } else {
        status_msg_ = "Usage: <pid> <priority>";
    }
    enable_raw_mode();
    std::cout << A::HIDE_CUR;
}

void UI::prompt_search() {
    disable_raw_mode();
    std::cout << A::SHOW_CUR << "\n";
    std::cout << A::ACCENT2 << "Search (name/command, blank to clear): " << A::RESET;
    std::string pat;
    std::getline(std::cin, pat);
    pat = Utils::trim(pat);

    if (pat.empty()) {
        pm_.clear_filter();
        status_msg_ = "Filter cleared.";
    } else {
        FilterOptions f;
        f.name_pattern = pat;
        pm_.set_filter(f);
        status_msg_ = "Filtering: " + pat;
    }
    scroll_offset_ = 0;
    enable_raw_mode();
    std::cout << A::HIDE_CUR;
}

void UI::prompt_filter() {
    disable_raw_mode();
    std::cout << A::SHOW_CUR << "\n";
    std::cout << A::ACCENT2 << "Filter user=<u> pid=<min>-<max> (blank=clear): " << A::RESET;
    std::string line;
    std::getline(std::cin, line);
    line = Utils::trim(line);

    if (line.empty()) {
        pm_.clear_filter();
        status_msg_ = "Filter cleared.";
    } else {
        FilterOptions f;
        auto tokens = Utils::split(line, ' ');
        for (const auto& tok : tokens) {
            if (tok.rfind("user=", 0) == 0) f.user_filter = tok.substr(5);
            if (tok.rfind("pid=",  0) == 0) {
                auto range = tok.substr(4);
                auto dash  = range.find('-');
                try {
                    f.pid_min = std::stoi(range.substr(0, dash));
                    if (dash != std::string::npos)
                        f.pid_max = std::stoi(range.substr(dash + 1));
                } catch (...) {}
            }
        }
        pm_.set_filter(f);
        status_msg_ = "Filter applied.";
    }
    scroll_offset_ = 0;
    enable_raw_mode();
    std::cout << A::HIDE_CUR;
}

void UI::cycle_sort() {
    switch (pm_.get_sort()) {
        case SortBy::CPU:    pm_.set_sort(SortBy::MEMORY); status_msg_ = "Sort: Memory"; break;
        case SortBy::MEMORY: pm_.set_sort(SortBy::PID);    status_msg_ = "Sort: PID";    break;
        case SortBy::PID:    pm_.set_sort(SortBy::NAME);   status_msg_ = "Sort: Name";   break;
        case SortBy::NAME:   pm_.set_sort(SortBy::CPU);    status_msg_ = "Sort: CPU";    break;
    }
}

// ── Formatting helpers ────────────────────────────────────────────────────────

std::string UI::truncate(const std::string& s, int max_len) const {
    if (max_len <= 0) return "";
    if ((int)s.size() <= max_len) return s;
    return s.substr(0, max_len - 1) + "~";
}

std::string UI::format_mem(long kb) const {
    return Utils::human_size(kb);
}
