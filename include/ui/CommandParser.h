#pragma once

#include "core/Types.h"
#include "thread/SharedQueue.h"

class PipeClient;

/// 前台命令解析器：读取 stdin → 解析 → 打包 Message → 投递（本地队列或 pipe）
class CommandParser {
public:
    CommandParser(SharedQueue& queue);
    CommandParser(PipeClient& pipe);

    /// 主循环（前台线程入口）
    void run();

    /// 命令名字符串 → MessageType 映射
    static MessageType parse_type(const std::string& cmd);

private:
    SharedQueue* queue_ = nullptr;
    PipeClient*  pipe_  = nullptr;

    /// 解析一行输入，返回 token 列表
    static std::vector<std::string> tokenize(const std::string& line);
};
