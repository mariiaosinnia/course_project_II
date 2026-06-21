#include "Server.h"
#include <iostream>

int main() {
    try {
        Server server;
        server.run();
    }
    catch (std::exception& e) {
        std::cerr << e.what() << "\n";
    }
    return 0;
}