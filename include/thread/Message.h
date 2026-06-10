#pragma once

#include "core/Types.h"

/// 消息结构：前台线程打包命令，后台线程消费处理
struct Message {
    MessageType              type = MessageType::INVALID;
    std::vector<std::string> args;

    std::promise<std::string> promise;
};
