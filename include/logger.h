#pragma once
#include <string>
#include <fstream>

class Logger {
public:
    explicit Logger(const std::string& path = "/tmp/proclite.log");
    ~Logger();

    void log_kill(int pid, const std::string& name, int sig);
    void log_cpu_spike(int pid, const std::string& name, float cpu_pct);
    void log(const std::string& msg);

    static Logger& instance();   // singleton for convenience

private:
    std::ofstream file_;
    std::string timestamp() const;
};
