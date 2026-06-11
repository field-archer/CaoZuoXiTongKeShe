#include "persist/StateFile.h"
#include "persist/Serializer.h"
#include "user/UserManager.h"
#include "core/Platform.h"
#include <cstring>
#include <sstream>

StateFile::StateFile(const std::string& path) : path_(path) {}

bool StateFile::exists() const { return Platform::file_exists(path_); }

bool StateFile::save(UserManager& um, const std::unordered_map<std::string, UserState>& users) {
    std::ostringstream buffer(std::ios::binary);
    Serializer::write_magic(buffer, MAGIC);// 文件类型
    Serializer::write_u32(buffer, VERSION);
    Serializer::write_u32(buffer, 0); // CRC placeholder
    Serializer::write_u64(buffer, Platform::current_time_ms());

    um.serialize(buffer);
    // 用户数 + 每个用户的 state
    uint32_t user_count = static_cast<uint32_t>(users.size());
    Serializer::write_u32(buffer, user_count);
    for (const auto& [name, u] : users) {
        uint16_t name_len = static_cast<uint16_t>(name.size());
        buffer.write(reinterpret_cast<const char*>(&name_len), sizeof(name_len));
        buffer.write(name.data(), name_len);
        u.pm.serialize(buffer);
        u.mm.serialize(buffer);
        u.mlfq.serialize(buffer);
    }

    const char end_marker[4] = {'E','N','D','\0'};
    Serializer::write_magic(buffer, end_marker);

    std::string data = buffer.str();
    std::string crc_data = data.substr(12);
    uint32_t crc = Serializer::crc32(crc_data);
    std::memcpy(&data[8], &crc, sizeof(crc));

    std::ofstream file(path_, std::ios::binary | std::ios::trunc);
    if (!file) return false;
    file.write(data.data(), data.size());
    return file.good();
}

bool StateFile::load(UserManager& um, std::unordered_map<std::string, UserState>& users) {
    if (!exists()) return false;
    std::ifstream file(path_, std::ios::binary);
    if (!file) return false;
    // 从文件初始化data
    std::string data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();
    if (data.size() < 20) return false;
    // 检查文件类型
    if (data[0]!=MAGIC[0] || data[1]!=MAGIC[1] || data[2]!=MAGIC[2] || data[3]!=MAGIC[3]) return false;
    // 检查版本
    uint32_t fver; std::memcpy(&fver, &data[4], sizeof(fver));
    if (fver != VERSION) return false;

    std::string crc_data = data.substr(12);
    uint32_t stored_crc, computed_crc;
    std::memcpy(&stored_crc, &data[8], sizeof(stored_crc));
    computed_crc = Serializer::crc32(crc_data);
    if (stored_crc != computed_crc) return false;

    std::istringstream iss(std::ios::binary);
    iss.str(data.substr(20));

    um.deserialize(iss);
    users.clear();
    uint32_t user_count = 0;
    user_count = Serializer::read_u32(iss);
    if (iss.fail()) return true; // 老格式，无用户 state

    for (uint32_t i = 0; i < user_count; ++i) {
        uint16_t name_len = 0;
        iss.read(reinterpret_cast<char*>(&name_len), sizeof(name_len));
        std::string name(name_len, '\0');
        iss.read(&name[0], name_len);

        UserState u;
        u.pm.deserialize(iss);
        u.mm.deserialize(iss);
        u.mlfq.deserialize(iss);
        u.init_scheduler(nullptr); // mutex 由 BackendEngine 后续设置
        users.emplace(name, std::move(u));
    }
    return !iss.fail();
}
