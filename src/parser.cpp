#include "parser.h"
#include "utils.h"

#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <optional>
#include <dirent.h>
#include <cctype>
#include <pwd.h>
#include <cstring>
#include <stdexcept>

namespace Parser {

// ─────────────────────────────────────────────
// /proc/[pid]/stat
// ─────────────────────────────────────────────
// Fields (1-indexed as in kernel docs):
//  1  pid  2  comm  3  state  4  ppid
// 14  utime  15  stime  16  cutime  17  cstime
// 18  priority  19  nice  20  num_threads
//
// 'comm' can contain spaces and is wrapped in parentheses.
// We parse by finding the last ')' to avoid tokenisation bugs.
std::optional<Process> parse_stat(int pid) {
    std::string path = "/proc/" + std::to_string(pid) + "/stat";
    std::ifstream f(path);
    if (!f.is_open()) return std::nullopt;

    std::string line;
    if (!std::getline(f, line)) return std::nullopt;

    // Find comm boundaries
    auto open_paren  = line.find('(');
    auto close_paren = line.rfind(')');
    if (open_paren == std::string::npos || close_paren == std::string::npos)
        return std::nullopt;

    Process p;
    try {
        p.pid  = std::stoi(line.substr(0, open_paren));
        p.name = line.substr(open_paren + 1, close_paren - open_paren - 1);

        // Everything after the closing paren
        std::string rest = line.substr(close_paren + 2);
        std::istringstream ss(rest);
        std::vector<std::string> fields;
        std::string tok;
        while (ss >> tok) fields.push_back(tok);

        // fields[0] = state (field 3)
        // fields[1] = ppid  (field 4)
        // fields[11]= utime (field 14)  → 0-indexed: 11
        // fields[12]= stime (field 15)
        // fields[13]= cutime(field 16)
        // fields[14]= cstime(field 17)
        // fields[15]= priority(field 18)
        // fields[16]= nice   (field 19)
        // fields[17]= num_threads (field 20)
        if (fields.size() < 18) return std::nullopt;

        char state_char = fields[0][0];
        p.state = Process::expand_state(state_char);
        p.ppid  = std::stoi(fields[1]);

        CpuSnapshot snap;
        snap.utime  = std::stol(fields[11]);
        snap.stime  = std::stol(fields[12]);
        snap.cutime = std::stol(fields[13]);
        snap.cstime = std::stol(fields[14]);
        p.prev_snap = snap;   // caller will treat this as "current" sample

        p.priority  = std::stoi(fields[16]); // nice
        p.threads   = std::stoi(fields[17]);
    } catch (...) {
        return std::nullopt;  // process vanished or malformed
    }

    return p;
}

// ─────────────────────────────────────────────
// /proc/[pid]/status
// ─────────────────────────────────────────────
bool parse_status(int pid, Process& proc) {
    std::string path = "/proc/" + std::to_string(pid) + "/status";
    std::ifstream f(path);
    if (!f.is_open()) return false;

    std::string line;
    while (std::getline(f, line)) {
        if (line.rfind("VmRSS:", 0) == 0) {
            std::istringstream ss(line.substr(6));
            ss >> proc.mem_rss_kb;
        } else if (line.rfind("Uid:", 0) == 0) {
            // Uid: real effective saved filesystem
            std::istringstream ss(line.substr(4));
            int uid = 0;
            ss >> uid;
            proc.user = uid_to_username(uid);
        } else if (line.rfind("Threads:", 0) == 0) {
            std::istringstream ss(line.substr(8));
            ss >> proc.threads;
        }
    }
    return true;
}

// ─────────────────────────────────────────────
// /proc/[pid]/cmdline
// ─────────────────────────────────────────────
std::string parse_cmdline(int pid) {
    std::string path = "/proc/" + std::to_string(pid) + "/cmdline";
    std::ifstream f(path);
    if (!f.is_open()) return "";

    std::string content;
    std::getline(f, content, '\0'); // read first null-terminated arg
    // Replace embedded nulls with spaces for the rest
    std::string result = content;
    std::string tok;
    while (std::getline(f, tok, '\0')) {
        if (!tok.empty()) result += ' ' + tok;
    }
    return result.empty() ? "[" + std::to_string(pid) + "]" : result;
}

// ─────────────────────────────────────────────
// /proc/stat  → aggregate CPU total time
// ─────────────────────────────────────────────
long parse_total_cpu_time() {
    std::ifstream f("/proc/stat");
    if (!f.is_open()) return 0;

    std::string line;
    std::getline(f, line);  // first line: "cpu  ..."
    if (line.rfind("cpu", 0) != 0) return 0;

    std::istringstream ss(line.substr(3));
    long total = 0, val = 0;
    while (ss >> val) total += val;
    return total;
}

// ─────────────────────────────────────────────
// /proc/meminfo  → total RAM in kB
// ─────────────────────────────────────────────
long parse_total_memory_kb() {
    std::ifstream f("/proc/meminfo");
    if (!f.is_open()) return 1; // avoid div/0

    std::string line;
    while (std::getline(f, line)) {
        if (line.rfind("MemTotal:", 0) == 0) {
            std::istringstream ss(line.substr(9));
            long val = 0;
            ss >> val;
            return val > 0 ? val : 1;
        }
    }
    return 1;
}

// ─────────────────────────────────────────────
// Enumerate PIDs
// ─────────────────────────────────────────────
std::vector<int> list_pids() {
    std::vector<int> pids;
    DIR* dir = opendir("/proc");
    if (!dir) return pids;

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_type == DT_DIR || entry->d_type == DT_UNKNOWN) {
            const char* n = entry->d_name;
            bool is_num = (n[0] != '\0');
            for (int i = 0; n[i]; ++i) {
                if (!std::isdigit(static_cast<unsigned char>(n[i]))) {
                    is_num = false; break;
                }
            }
            if (is_num) pids.push_back(std::atoi(n));
        }
    }
    closedir(dir);
    return pids;
}

// ─────────────────────────────────────────────
// UID → username
// ─────────────────────────────────────────────
std::string uid_to_username(int uid) {
    struct passwd* pw = getpwuid(static_cast<uid_t>(uid));
    return pw ? pw->pw_name : std::to_string(uid);
}

} // namespace Parser
