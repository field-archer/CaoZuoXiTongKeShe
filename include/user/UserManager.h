#pragma once

#include "core/Types.h"
#include "user/User.h"

/// 用户管理器：注册、登录、登出、锁定、会话隔离
class UserManager {
public:
    UserManager();

    // ── 命令行操作 ──
    std::string register_user(const std::string& username, const std::string& password);
    std::string login(const std::string& username, const std::string& password);
    std::string logout();

    // ── 查询 ──
    uint32_t current_uid() const { return session_uid_; }
    bool     is_logged_in() const { return session_uid_ != 0; }
    std::string current_username() const;
    void        restore_session(uint32_t uid);
    bool     is_locked(const std::string& username) const;

    // ── 持久化 ──
    void serialize(std::ostream& os) const;
    void deserialize(std::istream& is);

private:
    std::vector<User> users_;
    uint32_t          next_uid_    = 1;
    uint32_t          session_uid_ = 0;  // 0 = 未登录

    // 保证用户名唯一
    User* find_user(const std::string& username);
    const User* find_user(const std::string& username) const;
};
