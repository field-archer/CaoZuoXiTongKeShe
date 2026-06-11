#pragma once

#include "core/Types.h"
#include "user/User.h"

class UserManager {
public:
    UserManager();

    std::string register_user(const std::string& username, const std::string& password);
    std::string login(const std::string& username, const std::string& password);

    void serialize(std::ostream& os) const;
    void deserialize(std::istream& is);

private:
    std::vector<User> users_;
    uint32_t          next_uid_ = 1;

    User* find_user(const std::string& username);
    const User* find_user(const std::string& username) const;
};
