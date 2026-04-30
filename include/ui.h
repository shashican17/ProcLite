#pragma once
#include "process_manager.h"
#include <string>
#include <termios.h>

// All terminal I/O lives here.
// Uses raw ANSI escape codes — no ncurses dependency.
class UI {
public:
    UI(ProcessManager& pm);
    ~UI();

    // Main event loop: returns when user presses 'q'
    void run();

    // Startup configuration (call before run())
    void set_show_cores(bool v) { show_cores_ = v; }
    void set_show_tree(bool v)  { show_tree_  = v; }

private:
    ProcessManager& pm_;

    // Terminal state
    struct termios orig_termios_;
    bool           raw_mode_ = false;
    int            term_rows_ = 24;
    int            term_cols_ = 80;

    // UI state
    int            scroll_offset_ = 0;   // rows scrolled down
    std::string    status_msg_;          // bottom bar message
    bool           show_tree_   = false;
    bool           show_cores_  = false;

    // --- terminal helpers ---
    void enable_raw_mode();
    void disable_raw_mode();
    void get_terminal_size();
    char read_key();               // non-blocking; returns 0 if no key

    // --- rendering ---
    void draw();
    void draw_header();
    void draw_cpu_bar(float pct, int width, const std::string& label);
    void draw_process_table(const std::vector<Process>& procs);
    void draw_tree(int pid, const std::vector<Process>& all,
                   const std::string& prefix, bool last);
    void draw_footer();
    void clear_screen();

    // --- interactive actions ---
    void prompt_kill();
    void prompt_renice();
    void prompt_search();
    void prompt_filter();
    void cycle_sort();

    // --- formatting helpers ---
    std::string truncate(const std::string& s, int max_len) const;
    std::string bar_string(float pct, int width) const;
    std::string format_mem(long kb) const;
};
