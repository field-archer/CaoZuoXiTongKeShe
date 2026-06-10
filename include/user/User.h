#pragma once

#include "core/Types.h"

/// 用户账户数据结构
struct User {
    uint32_t    uid       = 0;
    std::string username;
    std::string password_hash;
    bool        locked    = false;
    int         failed_attempts = 0;
};
