#pragma once

#include "core/Types.h"
#include <sstream>

/// 二进制序列化工具
namespace Serializer {

/// 计算 CRC32（用于持久化文件校验）
uint32_t crc32(const std::string& data);
uint32_t crc32(const char* data, size_t len);

/// 写入 4 字节魔数
inline void write_magic(std::ostream& os, const char magic[4]) {
    os.write(magic, 4);
}

/// 验证魔数
inline bool check_magic(std::istream& is, const char magic[4]) {
    char buf[4];
    is.read(buf, 4);
    if (is.fail()) return false;
    return (buf[0] == magic[0] && buf[1] == magic[1]
         && buf[2] == magic[2] && buf[3] == magic[3]);
}

/// 写入/读取 uint32_t
inline void write_u32(std::ostream& os, uint32_t val) {
    os.write(reinterpret_cast<const char*>(&val), sizeof(val));
}
inline uint32_t read_u32(std::istream& is) {
    uint32_t val = 0;
    is.read(reinterpret_cast<char*>(&val), sizeof(val));
    return val;
}

/// 写入/读取 uint64_t
inline void write_u64(std::ostream& os, uint64_t val) {
    os.write(reinterpret_cast<const char*>(&val), sizeof(val));
}
inline uint64_t read_u64(std::istream& is) {
    uint64_t val = 0;
    is.read(reinterpret_cast<char*>(&val), sizeof(val));
    return val;
}

} // namespace Serializer
