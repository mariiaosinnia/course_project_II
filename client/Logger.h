#pragma once

#include <iostream>
#include <mutex>
#include <string>

// Потокобезпечний вивід у консоль. Мережевий потік (TCPClient callbacks)
// і потік CLI можуть писати в std::cout одночасно - без synchronized
// доступу рядки переплутаються між собою.
//
// Статичний mutex у .cpp гарантує єдиний екземпляр на весь процес,
// незалежно від того, з якого .cpp файлу викликається Logger::print.
class Logger {
public:
    static void print(const std::string& msg);

private:
    static std::mutex& mutex();
};
