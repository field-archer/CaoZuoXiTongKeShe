#pragma once

#include "core/Types.h"
#include "thread/Message.h"
#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

/// Named Pipe 客户端：前台进程连接 server，发送命令，接收结果
class PipeClient {
public:
    PipeClient(const std::string& pipe_name);
    ~PipeClient();

    /// 连接到 server
    bool connect();

    /// 发送一条 Message，返回 server 的响应
    std::string send(const Message& msg);

    /// 断开连接
    void disconnect();

    /// 是否已连接
    bool is_connected() const { return hPipe_ != INVALID_HANDLE_VALUE; }

private:
    std::string pipe_name_;
    HANDLE      hPipe_ = INVALID_HANDLE_VALUE;

    static std::string read_exact(HANDLE h, uint32_t len);
    static void write_exact(HANDLE h, const std::string& data);
};
