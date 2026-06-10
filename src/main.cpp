#include <iostream>
#include <string>
#include <cstring>

#include "core/Types.h"
#include "core/Config.h"
#include "core/Platform.h"
#include "thread/SharedQueue.h"
#include "thread/FileLock.h"
#include "ui/CommandParser.h"
#include "backend/BackendEngine.h"
#include "ipc/PipeClient.h"
#include <memory>

#ifdef _WIN32
#include <windows.h>
#endif

static void run_server(const std::string& state_file) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif
    std::cout << "[Server] 后台进程启动，状态文件: " << state_file << std::endl;

    SharedQueue queue;
    auto backend = std::make_shared<BackendEngine>(queue, state_file, false);

    std::cout << "[Server] 等待客户端连接..." << std::endl;

    std::string pipe_path = R"(\\.\pipe\OSCorePipe)";

    while (true) {
        HANDLE hPipe = CreateNamedPipeA(
            pipe_path.c_str(),
            PIPE_ACCESS_DUPLEX,
            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
            PIPE_UNLIMITED_INSTANCES,
            65536, 65536, 0, nullptr
        );
        if (hPipe == INVALID_HANDLE_VALUE) {
            Platform::sleep_ms(500);
            continue;
        }

        if (!ConnectNamedPipe(hPipe, nullptr)
            && GetLastError() != ERROR_PIPE_CONNECTED) {
            CloseHandle(hPipe);
            continue;
        }

        std::cout << "[Server] 客户端已连接。" << std::endl;

        // 每个客户端一个线程，独立 session
        std::thread([hPipe, backend]() {
            std::string session_user;
            while (true) {
                // 读 type
                uint32_t type_val = 0;
                DWORD n = 0;
                if (!ReadFile(hPipe, &type_val, 4, &n, nullptr) || n < 4) break;
                Message msg;
                msg.type = static_cast<MessageType>(type_val);

                // 读 arg count
                uint32_t argc = 0;
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

                // LOGIN/LOGOUT 跟踪本客户端 session
                if (msg.type == MessageType::LOGIN) {
                    auto r = backend->process(msg, "");
                    if (r.find("登录成功") != std::string::npos)
                        session_user = msg.args.empty() ? "" : msg.args[0];
                    uint32_t rlen = static_cast<uint32_t>(r.size());
                    DWORD w = 0;
                    WriteFile(hPipe, &rlen, 4, &w, nullptr);
                    WriteFile(hPipe, r.data(), rlen, &w, nullptr);
                    continue;
                }
                if (msg.type == MessageType::LOGOUT) {
                    session_user.clear();
                    std::string r = "已登出。";
                    uint32_t rlen = static_cast<uint32_t>(r.size());
                    DWORD w = 0;
                    WriteFile(hPipe, &rlen, 4, &w, nullptr);
                    WriteFile(hPipe, r.data(), rlen, &w, nullptr);
                    continue;
                }

                std::string result = backend->process(msg, session_user);

                // 发送结果: [4B len] [data]
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
    std::string mode = "standalone";
    std::string state_file = DEFAULT_STATE_FILE;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--server") == 0)
            mode = "server";
        else if (std::strcmp(argv[i], "--client") == 0)
            mode = "client";
        else
            state_file = argv[i];
    }

    if (mode == "server") {
        run_server(state_file);
    } else if (mode == "client") {
        run_client();
    } else {
        // standalone 模式：本地前后台线程
#ifdef _WIN32
        SetConsoleOutputCP(CP_UTF8);
#endif
        std::cout << "╔══════════════════════════════════════════════╗" << std::endl;
        std::cout << "║   操作系统核心模拟器 v1.0.0                    ║" << std::endl;
        std::cout << "║   可持久化 | MLFQ调度 | 动态分区内存       ║" << std::endl;
        std::cout << "╚══════════════════════════════════════════════╝" << std::endl;

        SharedQueue queue;
        BackendEngine backend(queue, state_file, true);
        std::thread backend_thread([&]() { backend.run(); });

        {
            CommandParser parser(queue);
            parser.run();
        }

        queue.shutdown();
        if (backend_thread.joinable()) backend_thread.join();
        std::cout << "[INFO] 系统已退出。" << std::endl;
    }

    return 0;
}
