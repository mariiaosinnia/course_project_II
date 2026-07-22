#pragma once

#include <mutex>
#include <string>

class Logger {
public:
    static void print(const std::string& msg);

private:
    static std::mutex& mutex();
};
