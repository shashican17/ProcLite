#include "logger.h"
#include <ctime>
#include <sstream>
#include <iomanip>
#include <csignal>

Logger::Logger(const std::string& path) {
    file_.open(path, std::ios::app);
}

Logger::~Logger() {
    if (file_.is_open()) file_.close();
}

Logger& Logger::instance() {
    static Logger inst("/tmp/proclite.log");
    return inst;
}

std::string Logger::timestamp() const {
    std::time_t t = std::time(nullptr);
    std::tm* tm_info = std::localtime(&t);
    std::ostringstream ss;
    ss << std::put_time(tm_info, "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

void Logger::log(const std::string& msg) {
    if (file_.is_open())
        file_ << "[" << timestamp() << "] " << msg << "\n";
}

void Logger::log_kill(int pid, const std::string& name, int sig) {
    std::string sig_name = (sig == SIGTERM) ? "SIGTERM" :
                           (sig == SIGKILL) ? "SIGKILL" :
                           "SIG" + std::to_string(sig);
    log("KILL  pid=" + std::to_string(pid) +
        " name=" + name + " signal=" + sig_name);
}

void Logger::log_cpu_spike(int pid, const std::string& name, float cpu_pct) {
    std::ostringstream ss;
    ss << "SPIKE pid=" << pid << " name=" << name
       << " cpu=" << std::fixed << std::setprecision(1) << cpu_pct << "%";
    log(ss.str());
}
