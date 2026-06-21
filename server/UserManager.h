#pragma once
#include <unordered_map>
#include <memory>
#include <shared_mutex>

#include "User.h"

class UserManager {
public:
    User* add(uint32_t id);
    void remove(uint32_t id);
    User* get(uint32_t id) const;

private:
    mutable std::shared_mutex mutex;
    std::unordered_map<uint32_t, std::unique_ptr<User>> users;
};