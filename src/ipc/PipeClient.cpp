#include "ipc/PipeClient.h"
#include <cstring>
#include <vector>

PipeClient::PipeClient(const std::string& name) : pipe_name_(name) {}

PipeClient::~PipeClient() { disconnect(); }

bool PipeClient::connect() {
    std::string full_name = R"(\\.\pipe\)" + pipe_name_;
    hPipe_ = CreateFileA(
        full_name.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        0, nullptr, OPEN_EXISTING, 0, nullptr
    );
    if (hPipe_ == INVALID_HANDLE_VALUE) return false;
    DWORD mode = PIPE_READMODE_MESSAGE;
    SetNamedPipeHandleState(hPipe_, &mode, nullptr, nullptr);
    return true;
}

std::string PipeClient::send(const Message& msg) {
    if (hPipe_ == INVALID_HANDLE_VALUE) return "Error: 未连接到 server。";

    // 写 type (4B)
    uint32_t type_val = static_cast<uint32_t>(msg.type);
    write_exact(hPipe_, std::string(reinterpret_cast<const char*>(&type_val), 4));

    // 写 arg_count (4B)
    uint32_t arg_count = static_cast<uint32_t>(msg.args.size());
    write_exact(hPipe_, std::string(reinterpret_cast<const char*>(&arg_count), 4));

    // 写每个 arg
    for (const auto& a : msg.args) {
        uint32_t alen = static_cast<uint32_t>(a.size());
        write_exact(hPipe_, std::string(reinterpret_cast<const char*>(&alen), 4));
        write_exact(hPipe_, a);
    }

    // 读响应长度
    std::string len_bytes = read_exact(hPipe_, 4);
    if (len_bytes.size() < 4) return "Error: server 无响应。";
    uint32_t result_len = *reinterpret_cast<const uint32_t*>(len_bytes.data());

    // 读响应体
    return read_exact(hPipe_, result_len);
}

void PipeClient::disconnect() {
    if (hPipe_ != INVALID_HANDLE_VALUE) {
        CloseHandle(hPipe_);
        hPipe_ = INVALID_HANDLE_VALUE;
    }
}

std::string PipeClient::read_exact(HANDLE h, uint32_t len) {
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

void PipeClient::write_exact(HANDLE h, const std::string& data) {
    DWORD total = 0;
    uint32_t len = static_cast<uint32_t>(data.size());
    while (total < len) {
        DWORD n = 0;
        if (!WriteFile(h, data.data() + total, len - total, &n, nullptr)) break;
        total += n;
    }
}
