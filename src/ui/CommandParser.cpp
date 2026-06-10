#include "ui/CommandParser.h"
#include "ipc/PipeClient.h"
#include <sstream>
#include <iostream>
#include <algorithm>
#include <unordered_map>

CommandParser::CommandParser(SharedQueue& queue) : queue_(&queue) {}
CommandParser::CommandParser(PipeClient& pipe)   : pipe_(&pipe) {}

MessageType CommandParser::parse_type(const std::string& cmd) {
    static const std::unordered_map<std::string, MessageType> map = {
        {"register",      MessageType::REGISTER},
        {"login",         MessageType::LOGIN},
        {"logout",        MessageType::LOGOUT},
        {"create_pcb",    MessageType::CREATE_PCB},
        {"kill_pcb",      MessageType::KILL_PCB},
        {"block_pcb",     MessageType::BLOCK_PCB},
        {"wakeup_pcb",    MessageType::WAKEUP_PCB},
        {"show_pcb",      MessageType::SHOW_PCB},
        {"list_pcb",      MessageType::LIST_PCB},
        {"ptree",         MessageType::PTREE},
        {"suspend",       MessageType::SUSPEND},
        {"resume",        MessageType::RESUME},
        {"renice",        MessageType::RENICE},
        {"start_sched",   MessageType::START_SCHED},
        {"stop_sched",    MessageType::STOP_SCHED},
        {"restart_sched", MessageType::RESTART_SCHED},
        {"step",          MessageType::STEP},
        {"alloc",         MessageType::ALLOC},
        {"free_mem",      MessageType::FREE_MEM},
        {"show_mem",      MessageType::SHOW_MEM},
        {"compact",       MessageType::COMPACT},
        {"mem_stat",      MessageType::MEM_STAT},
        {"set_alloc_algo", MessageType::SET_ALLOC_ALGO},
        {"pgfault",       MessageType::PGFAULT},
        {"swap_out",      MessageType::SWAP_OUT},
        {"save",          MessageType::SAVE},
        {"load",          MessageType::LOAD},
        {"overview",      MessageType::OVERVIEW},
        {"help",          MessageType::HELP},
        {"exit",          MessageType::EXIT},
    };
    auto it = map.find(cmd);
    return (it != map.end()) ? it->second : MessageType::INVALID;
}

std::vector<std::string> CommandParser::tokenize(const std::string& line) {
    std::vector<std::string> tokens;
    std::istringstream iss(line);
    std::string token;
    while (iss >> token) tokens.push_back(token);
    return tokens;
}

void CommandParser::run() {
    std::cout << "╔══════════════════════════════════════════════╗" << std::endl;
    std::cout << "║   操作系统核心模拟器 v1.0.0                    ║" << std::endl;
    std::cout << "║   可持久化 | MLFQ调度 | 动态分区内存       ║" << std::endl;
    std::cout << "╚══════════════════════════════════════════════╝" << std::endl;
    std::cout << "输入 'help' 查看命令列表。" << std::endl << std::endl;

    std::string line;
    while (true) {
        std::cout << "> ";
        if (!std::getline(std::cin, line)) break;
        if (line.empty()) continue;
        if (line[0] == '#') continue;

        auto tokens = tokenize(line);
        if (tokens.empty()) continue;

        MessageType type = parse_type(tokens[0]);
        if (type == MessageType::INVALID) {
            std::cout << "未知命令: " << tokens[0] << "。输入 'help' 查看命令列表。" << std::endl;
            continue;
        }

        if (type == MessageType::EXIT) {
            std::cout << "再见。" << std::endl;
            if (queue_) {
                Message msg; msg.type = MessageType::EXIT;
                queue_->push(std::move(msg));
            }
            break;
        }

        Message msg;
        msg.type = type;
        for (size_t i = 1; i < tokens.size(); ++i)
            msg.args.push_back(tokens[i]);

        std::string result;
        if (queue_) {
            // standalone 模式：通过 SharedQueue
            auto future = msg.promise.get_future();
            queue_->push(std::move(msg));
            result = future.get();
        } else if (pipe_) {
            // client 模式：通过 Pipe
            result = pipe_->send(msg);
        }

        std::cout << result << std::endl << std::endl;
    }
}
