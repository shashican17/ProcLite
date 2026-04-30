#include "process_manager.h"
#include "ui.h"

#include <iostream>
#include <string>
#include <csignal>
#include <cstdlib>
#include <unistd.h>
#include <termios.h>

static struct termios g_orig_termios;
static bool           g_raw_active = false;

static void on_signal(int) {
    if (g_raw_active)
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &g_orig_termios);
    std::cout << "\033[?25h\033[0m\n";
    std::exit(0);
}

static void print_help(const char* argv0) {
    std::cout <<
"ProcLite - Lightweight Linux Process Manager\n\n"
"Usage:\n"
"  " << argv0 << " [options]\n\n"
"Options:\n"
"  --sort cpu|mem|pid|name   Initial sort column  (default: cpu)\n"
"  --filter <pattern>        Pre-set name/command filter\n"
"  --user <username>         Show only processes for that user\n"
"  --cores                   Show per-core CPU bars on startup\n"
"  --tree                    Start in process-tree view\n"
"  -h, --help                Show this help\n\n"
"Interactive keys:\n"
"  q          Quit\n"
"  k          Kill process (SIGTERM)  |  'f <pid>' for SIGKILL\n"
"  r          Renice process\n"
"  /          Search by name/command\n"
"  F          Advanced filter (user=, pid=min-max)\n"
"  s          Cycle sort column\n"
"  t          Toggle tree view\n"
"  c          Toggle per-core CPU bars\n"
"  j / u      Scroll table down / up\n";
}

int main(int argc, char* argv[]) {
    SortBy        initial_sort = SortBy::CPU;
    FilterOptions initial_filter;
    bool          show_cores  = false;
    bool          show_tree   = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            print_help(argv[0]);
            return 0;
        } else if ((arg == "--sort") && i + 1 < argc) {
            std::string s = argv[++i];
            if      (s == "cpu")  initial_sort = SortBy::CPU;
            else if (s == "mem")  initial_sort = SortBy::MEMORY;
            else if (s == "pid")  initial_sort = SortBy::PID;
            else if (s == "name") initial_sort = SortBy::NAME;
            else { std::cerr << "Unknown sort key: " << s << "\n"; return 1; }
        } else if ((arg == "--filter") && i + 1 < argc) {
            initial_filter.name_pattern = argv[++i];
        } else if ((arg == "--user") && i + 1 < argc) {
            initial_filter.user_filter = argv[++i];
        } else if (arg == "--cores") {
            show_cores = true;
        } else if (arg == "--tree") {
            show_tree = true;
        } else {
            std::cerr << "Unknown option: " << arg << "\n";
            print_help(argv[0]);
            return 1;
        }
    }

    // Save original terminal state for signal handler
    tcgetattr(STDIN_FILENO, &g_orig_termios);
    g_raw_active = true;

    struct sigaction sa{};
    sa.sa_handler = on_signal;
    sigaction(SIGINT,  &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);

    ProcessManager pm;
    pm.set_sort(initial_sort);
    if (!initial_filter.name_pattern.empty() || !initial_filter.user_filter.empty())
        pm.set_filter(initial_filter);

    UI ui(pm);
    ui.set_show_cores(show_cores);
    ui.set_show_tree(show_tree);
    ui.run();

    g_raw_active = false;
    return 0;
}
