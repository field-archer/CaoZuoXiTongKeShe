#include "user/UserManager.h"
#include "core/Platform.h"
#include "core/Config.h"
#include <sstream>

UserManager::UserManager() {
    // 默认管理员账户
    User admin;
    admin.uid       = next_uid_++;
    admin.username  = "admin";
    admin.password_hash = Platform::hash_password("admin");
    users_.push_back(admin);
}

// ── 辅助 ──
User* UserManager::find_user(const std::string& username) {
    for (auto& u : users_) {
        if (u.username == username) return &u;
    }
    return nullptr;
}

const User* UserManager::find_user(const std::string& username) const {
    for (const auto& u : users_) {
        if (u.username == username) return &u;
    }
    return nullptr;
}

// ── 注册 ──
std::string UserManager::register_user(const std::string& username, const std::string& password) {
    if (username.empty() || password.empty()) {
        return "Error: 用户名和密码不能为空。";
    }
    if (find_user(username)) {
        return "Error: 用户名 '" + username + "' 已存在。";
    }

    User u;
    u.uid           = next_uid_++;
    u.username      = username;
    u.password_hash = Platform::hash_password(password);
    users_.push_back(u);

    std::ostringstream oss;
    oss << u.username << "(" << u.uid << ") 注册成功。";
    return oss.str();
}

// ── 登录 ──
std::string UserManager::login(const std::string& username, const std::string& password) {
    User* u = find_user(username);
    if (!u) {
        return "Error: 用户 '" + username + "' 不存在。";
    }
    if (u->locked) {
        return "Error: 账户 '" + username + "' 已被锁定，请联系管理员。";
    }

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

    // 登录成功
    u->failed_attempts = 0;
    session_uid_ = u->uid;

    std::ostringstream oss;
    oss << u->username << "(" << u->uid << ") 登录成功。";
    return oss.str();
}

// ── 登出 ──
std::string UserManager::logout() {
    if (!is_logged_in()) {
        return "Error: 当前未登录。";
    }
    uint32_t uid = session_uid_;
    session_uid_ = 0;
    return "已登出（UID=" + std::to_string(uid) + "）。";
}

// ── 查询 ──
std::string UserManager::current_username() const {
    for (const auto& u : users_)
        if (u.uid == session_uid_) return u.username;
    return "";
}

void UserManager::restore_session(uint32_t uid) {
    for (const auto& u : users_)
        if (u.uid == uid) { session_uid_ = uid; return; }
    session_uid_ = 0; // 用户不存在了
}

bool UserManager::is_locked(const std::string& username) const {
    const User* u = find_user(username);
    return u && u->locked;
}

// ── 持久化 ──
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

    session_uid_ = 0; // 加载状态后需要重新登录
}
