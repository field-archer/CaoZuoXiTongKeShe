#include <iostream>
#include <string>
#include <cstring>
#include <memory>

#include "core/Config.h"
#include "core/Platform.h"
#include "ui/CommandParser.h"
#include "backend/BackendEngine.h"
#include "ipc/PipeClient.h"

#ifdef _WIN32
#include <windows.h>
#endif

static void run_server(const std::string& state_file) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif
    std::cout << "[Server] 状态文件: " << state_file << std::endl;

    auto backend = std::make_shared<BackendEngine>(state_file);
    std::string pipe_path = R"(\\.\pipe\OSCorePipe)";

    std::cout << "[Server] 等待客户端连接..." << std::endl;
    while (true) {
        HANDLE hPipe = CreateNamedPipeA(
            pipe_path.c_str(),
            PIPE_ACCESS_DUPLEX,
            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
            PIPE_UNLIMITED_INSTANCES, 65536, 65536, 0, nullptr);
        if (hPipe == INVALID_HANDLE_VALUE) { Platform::sleep_ms(500); continue; }
        if (!ConnectNamedPipe(hPipe, nullptr) && GetLastError() != ERROR_PIPE_CONNECTED) {
            CloseHandle(hPipe); continue;
        }
        std::cout << "[Server] 客户端已连接。" << std::endl;

        std::thread([hPipe, backend]() {
            std::string session_user;
            while (true) {
                DWORD n = 0;
                uint32_t type_val = 0, argc = 0;
                if (!ReadFile(hPipe, &type_val, 4, &n, nullptr) || n < 4) break;
                Message msg;
                msg.type = static_cast<MessageType>(type_val);
                if (!ReadFile(hPipe, &argc, 4, &n, nullptr) || n < 4) break;
                for (uint32_t i = 0; i < argc; ++i) {
                    uint32_t alen = 0;
                    if (!ReadFile(hPipe, &alen, 4, &n, nullptr) || n < 4) break;
                    std::string arg(alen, '\0');
                    DWORD total = 0;
                    while (total < alen) {
                        if (!ReadFile(hPipe, &arg[total], alen - total, &n, nullptr) || n == 0) break;
                        total += n;
                    }
                    arg.resize(total);
                    msg.args.push_back(arg);
                }
                if (msg.type == MessageType::EXIT || msg.type == MessageType::INVALID) break;

                // 跟踪 session
                if (msg.type == MessageType::LOGIN) {
                    auto r = backend->process(msg, "");
                    if (r.find("登录成功") != std::string::npos)
                        session_user = msg.args.empty() ? "" : msg.args[0];
                    uint32_t rlen = static_cast<uint32_t>(r.size());
                    WriteFile(hPipe, &rlen, 4, &n, nullptr);
                    WriteFile(hPipe, r.data(), rlen, &n, nullptr);
                    continue;
                }
                if (msg.type == MessageType::LOGOUT) {
                    session_user.clear();
                    std::string r = "已登出。";
                    uint32_t rlen = static_cast<uint32_t>(r.size());
                    WriteFile(hPipe, &rlen, 4, &n, nullptr);
                    WriteFile(hPipe, r.data(), rlen, &n, nullptr);
                    continue;
                }

                std::string result = backend->process(msg, session_user);
                uint32_t rlen = static_cast<uint32_t>(result.size());
                WriteFile(hPipe, &rlen, 4, &n, nullptr);
                DWORD wtotal = 0;
                while (wtotal < rlen) {
                    if (!WriteFile(hPipe, result.data() + wtotal, rlen - wtotal, &n, nullptr)) break;
                    wtotal += n;
                }
            }
            FlushFileBuffers(hPipe);
            DisconnectNamedPipe(hPipe);
            CloseHandle(hPipe);
            std::cout << "[Server] 客户端断开。" << std::endl;
        }).detach();
    }
}

static void run_client() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif
    PipeClient pipe("OSCorePipe");
    if (!pipe.connect()) {
        std::cout << "Error: 无法连接到后台进程。请先启动 server。" << std::endl;
        return;
    }
    std::cout << "已连接到 server。" << std::endl;
    CommandParser parser(pipe);
    parser.run();
}

int main(int argc, char* argv[]) {
    std::string state_file = DEFAULT_STATE_FILE;
    bool is_server = false, is_client = false;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--server") == 0) is_server = true;
        else if (std::strcmp(argv[i], "--client") == 0) is_client = true;
        else state_file = argv[i];
    }
    if (is_server) run_server(state_file);
    else if (is_client) run_client();
    else {
        std::cout << "用法: OSCoreSimulator --server | --client [状态文件]" << std::endl;
        return 1;
    }
    return 0;
}
