#include "user/UserManager.h"
#include "core/Platform.h"
#include "core/Config.h"
#include <sstream>

UserManager::UserManager() {
    User admin;
    admin.uid = next_uid_++;
    admin.username = "admin";
    admin.password_hash = Platform::hash_password("admin");
    users_.push_back(admin);
}

User* UserManager::find_user(const std::string& username) {
    for (auto& u : users_)
        if (u.username == username) return &u;
    return nullptr;
}

const User* UserManager::find_user(const std::string& username) const {
    for (const auto& u : users_)
        if (u.username == username) return &u;
    return nullptr;
}

std::string UserManager::register_user(const std::string& username, const std::string& password) {
    if (username.empty() || password.empty())
        return "Error: 用户名和密码不能为空。";
    if (find_user(username))
        return "Error: 用户名 '" + username + "' 已存在。";
    User u;
    u.uid = next_uid_++;
    u.username = username;
    u.password_hash = Platform::hash_password(password);
    users_.push_back(u);
    std::ostringstream oss;
    oss << u.username << "(" << u.uid << ") 注册成功。";
    return oss.str();
}

std::string UserManager::login(const std::string& username, const std::string& password) {
    User* u = find_user(username);
    if (!u) return "Error: 用户 '" + username + "' 不存在。";
    if (u->locked) return "Error: 账户 '" + username + "' 已被锁定。";
    std::string hash = Platform::hash_password(password);
    if (u->password_hash != hash) {
        u->failed_attempts++;
        if (u->failed_attempts >= MAX_LOGIN_ATTEMPTS) {
            u->locked = true;
            return "Error: 密码错误。已达 " + std::to_string(MAX_LOGIN_ATTEMPTS)
                   + " 次上限，账户已锁定。";
        }
        return "Error: 密码错误。剩余尝试次数: "
               + std::to_string(MAX_LOGIN_ATTEMPTS - u->failed_attempts);
    }
    u->failed_attempts = 0;
    std::ostringstream oss;
    oss << u->username << "(" << u->uid << ") 登录成功。";
    return oss.str();
}

void UserManager::serialize(std::ostream& os) const {
    uint32_t count = static_cast<uint32_t>(users_.size());
    os.write(reinterpret_cast<const char*>(&count), sizeof(count));
    os.write(reinterpret_cast<const char*>(&next_uid_), sizeof(next_uid_));
    for (const auto& u : users_) {
        os.write(reinterpret_cast<const char*>(&u.uid), sizeof(u.uid));
        uint16_t name_len = static_cast<uint16_t>(u.username.size());
        os.write(reinterpret_cast<const char*>(&name_len), sizeof(name_len));
        os.write(u.username.data(), name_len);
        uint16_t pass_len = static_cast<uint16_t>(u.password_hash.size());
        os.write(reinterpret_cast<const char*>(&pass_len), sizeof(pass_len));
        os.write(u.password_hash.data(), pass_len);
        uint8_t locked_byte = u.locked ? 1 : 0;
        os.write(reinterpret_cast<const char*>(&locked_byte), sizeof(locked_byte));
        os.write(reinterpret_cast<const char*>(&u.failed_attempts), sizeof(u.failed_attempts));
    }
}

void UserManager::deserialize(std::istream& is) {
    users_.clear();
    uint32_t count = 0;
    is.read(reinterpret_cast<char*>(&count), sizeof(count));
    if (is.fail()) return;
    is.read(reinterpret_cast<char*>(&next_uid_), sizeof(next_uid_));
    for (uint32_t i = 0; i < count; ++i) {
        User u;
        is.read(reinterpret_cast<char*>(&u.uid), sizeof(u.uid));
        uint16_t name_len = 0;
        is.read(reinterpret_cast<char*>(&name_len), sizeof(name_len));
        u.username.resize(name_len);
        is.read(&u.username[0], name_len);
        uint16_t pass_len = 0;
        is.read(reinterpret_cast<char*>(&pass_len), sizeof(pass_len));
        u.password_hash.resize(pass_len);
        is.read(&u.password_hash[0], pass_len);
        uint8_t locked_byte = 0;
        is.read(reinterpret_cast<char*>(&locked_byte), sizeof(locked_byte));
        u.locked = (locked_byte != 0);
        is.read(reinterpret_cast<char*>(&u.failed_attempts), sizeof(u.failed_attempts));
        users_.push_back(u);
    }
}
