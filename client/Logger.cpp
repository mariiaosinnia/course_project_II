#include "Logger.h"

#include <fstream>

std::mutex& Logger::mutex()
{
    static std::mutex m;
    return m;
}

void Logger::print(const std::string& msg)
{
    std::lock_guard<std::mutex> lock(mutex());
    std::ofstream log("client.log", std::ios::app);
    log << msg << "\n";
}
