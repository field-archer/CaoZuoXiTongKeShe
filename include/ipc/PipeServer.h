#pragma once

#include "core/Types.h"
#include "thread/Message.h"
#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

/// Named Pipe 服务端：后台进程监听，接收 client 命令，返回结果
class PipeServer {
public:
    PipeServer(const std::string& pipe_name);
    ~PipeServer();

    /// 等待一个客户端连接
    bool wait_for_client();

    /// 从已连接的客户端读取一条 Message
    Message read_message();

    /// 向客户端发送结果字符串
    void send_result(const std::string& result);

    /// 断开当前客户端
    void disconnect();

    /// 是否已连接客户端
    bool is_connected() const { return client_connected_; }

private:
    std::string pipe_name_;
    HANDLE      hPipe_       = INVALID_HANDLE_VALUE;
    bool        client_connected_ = false;

    static std::string read_exact(HANDLE h, uint32_t len);
    static void write_exact(HANDLE h, const std::string& data);
};
