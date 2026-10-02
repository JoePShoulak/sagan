#pragma once

#include <cstdint>
#include <string>

auto sagan_5f5f72656e6465725f77696e646f775f6f70656e(
    const std::string &title, std::int64_t width, std::int64_t height) -> bool;
auto sagan_5f5f72656e6465725f77696e646f775f706f6c6c() -> bool;
auto sagan_5f5f72656e6465725f77696e646f775f636c656172(
    std::int64_t red, std::int64_t green, std::int64_t blue) -> void;
auto sagan_5f5f72656e6465725f77696e646f775f636c6f7365() -> void;
