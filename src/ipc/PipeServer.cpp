#include "ipc/PipeServer.h"
#include <cstring>
#include <vector>

PipeServer::PipeServer(const std::string& name) : pipe_name_(name) {}

PipeServer::~PipeServer() {
    disconnect();
}

bool PipeServer::wait_for_client() {
    disconnect();
    std::string full_name = R"(\\.\pipe\)" + pipe_name_;

    hPipe_ = CreateNamedPipeA(
        full_name.c_str(),
        PIPE_ACCESS_DUPLEX,
        PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
        1,                     // 单客户端
        65536, 65536,          // 缓冲区
        0, nullptr
    );
    if (hPipe_ == INVALID_HANDLE_VALUE) return false;

    if (!ConnectNamedPipe(hPipe_, nullptr)) {
        if (GetLastError() != ERROR_PIPE_CONNECTED) {
            CloseHandle(hPipe_);
            hPipe_ = INVALID_HANDLE_VALUE;
            return false;
        }
    }
    client_connected_ = true;
    return true;
}

Message PipeServer::read_message() {
    Message msg;
    // 读 type (4B)
    uint32_t type_val = 0;
    std::string type_bytes = read_exact(hPipe_, 4);
    if (type_bytes.size() < 4) { msg.type = MessageType::EXIT; return msg; }
    type_val = *reinterpret_cast<const uint32_t*>(type_bytes.data());
    msg.type = static_cast<MessageType>(type_val);

    // 读 arg_count (4B)
    uint32_t arg_count = 0;
    std::string count_bytes = read_exact(hPipe_, 4);
    if (count_bytes.size() < 4) return msg;
    arg_count = *reinterpret_cast<const uint32_t*>(count_bytes.data());

    for (uint32_t i = 0; i < arg_count; ++i) {
        uint32_t arg_len = 0;
        std::string len_bytes = read_exact(hPipe_, 4);
        if (len_bytes.size() < 4) break;
        arg_len = *reinterpret_cast<const uint32_t*>(len_bytes.data());
        msg.args.push_back(read_exact(hPipe_, arg_len));
    }
    return msg;
}

void PipeServer::send_result(const std::string& result) {
    uint32_t len = static_cast<uint32_t>(result.size());
    std::string header(4, '\0');
    std::memcpy(&header[0], &len, 4);
    write_exact(hPipe_, header);
    write_exact(hPipe_, result);
}

void PipeServer::disconnect() {
    if (hPipe_ != INVALID_HANDLE_VALUE) {
        FlushFileBuffers(hPipe_);
        DisconnectNamedPipe(hPipe_);
        CloseHandle(hPipe_);
        hPipe_ = INVALID_HANDLE_VALUE;
    }
    client_connected_ = false;
}

std::string PipeServer::read_exact(HANDLE h, uint32_t len) {
    std::string buf(len, '\0');
    DWORD total = 0;
    while (total < len) {
        DWORD n = 0;
        if (!ReadFile(h, &buf[total], len - total, &n, nullptr) || n == 0) break;
        total += n;
    }
    buf.resize(total);
    return buf;
}

void PipeServer::write_exact(HANDLE h, const std::string& data) {
    DWORD total = 0;
    uint32_t len = static_cast<uint32_t>(data.size());
    while (total < len) {
        DWORD n = 0;
        if (!WriteFile(h, data.data() + total, len - total, &n, nullptr)) break;
        total += n;
    }
}
