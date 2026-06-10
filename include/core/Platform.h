#pragma once

#include <cstdint>
#include <string>

// ── 平台工具函数（Windows）──

namespace Platform {

/// 休眠指定毫秒数
void sleep_ms(uint32_t ms);

/// 获取文件的最后修改时间戳（毫秒 since epoch），失败返回 0
uint64_t file_last_write_time_ms(const std::string& path);

/// 检查文件是否存在
bool file_exists(const std::string& path);

/// 获取当前时间戳（毫秒 since epoch）
uint64_t current_time_ms();

/// 简单哈希字符串（用于密码存储）
std::string hash_password(const std::string& password);

} // namespace Platform
