#pragma once

#include "core/Types.h"

class PipeClient;

class CommandParser {
public:
    CommandParser(PipeClient& pipe);
    void run();
    static MessageType parse_type(const std::string& cmd);

private:
    PipeClient& pipe_;
    static std::vector<std::string> tokenize(const std::string& line);
};
