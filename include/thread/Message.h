#pragma once

#include "core/Types.h"

/// 命令消息
struct Message {
    MessageType              type = MessageType::INVALID;
    std::vector<std::string> args;
};
