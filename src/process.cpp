#include "process.h"

std::string Process::expand_state(char c) {
    switch (c) {
        case 'R': return "Running";
        case 'S': return "Sleeping";
        case 'D': return "Disk Sleep";
        case 'Z': return "Zombie";
        case 'T': return "Stopped";
        case 't': return "Tracing";
        case 'X': return "Dead";
        case 'I': return "Idle";
        default:  return std::string(1, c);
    }
}
