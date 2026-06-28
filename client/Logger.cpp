#include "Logger.h"

std::mutex& Logger::mutex()
{
    static std::mutex m;
    return m;
}

void Logger::print(const std::string& msg)
{
    std::lock_guard<std::mutex> lock(mutex());
    std::cout << msg << "\n";
}
