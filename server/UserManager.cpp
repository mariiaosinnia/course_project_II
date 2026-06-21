#include "UserManager.h"


User* UserManager::add(uint32_t id) {
    std::unique_lock lock(mutex);
    std::unique_ptr<User> user = std::make_unique<User>();
    user->id = id;
    user->room_id = 0;
    users.emplace(id, std::move(user));
    return users.at(id).get();
}

void UserManager::remove(uint32_t id) {
    std::unique_lock lock(mutex);
    users.erase(id);
}

User* UserManager::get(uint32_t id) const {
    std::shared_lock lock(mutex);
    auto it = users.find(id);
    if (it == users.end()) {
        return nullptr;
    }
    return it->second.get();
}