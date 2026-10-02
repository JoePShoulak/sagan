#pragma once

#include <cstdint>
#include <string>

auto sagan_5f5f72656e6465725f77696e646f775f6f70656e(
    const std::string &title, std::int64_t width, std::int64_t height) -> bool;
auto sagan_5f5f72656e6465725f77696e646f775f706f6c6c() -> bool;
auto sagan_5f5f72656e6465725f77696e646f775f636c656172(
    std::int64_t red, std::int64_t green, std::int64_t blue) -> void;
auto sagan_5f5f72656e6465725f77696e646f775f636c6f7365() -> void;
auto sagan_5f5f72656e6465725f656c61707365645f7365636f6e6473() -> double;
auto sagan_5f5f72656e6465725f6b65795f70726573736564(const std::string &key) -> bool;
auto sagan_5f5f72656e6465725f7363726f6c6c5f79() -> double;
auto sagan_5f5f72656e6465725f7365745f76696577(double center_x, double center_y,
                                             double pixels_per_unit) -> void;
auto sagan_5f5f72656e6465725f70726573656e74() -> void;
auto sagan_5f5f72656e6465725f636972636c65(
    double x, double y, double radius, std::int64_t red, std::int64_t green,
    std::int64_t blue) -> void;
auto sagan_5f5f72656e6465725f6c696e65(
    double start_x, double start_y, double end_x, double end_y, double width,
    std::int64_t red, std::int64_t green, std::int64_t blue) -> void;
auto sagan_5f5f72656e6465725f74657874(
    double x, double y, const std::string &value, std::int64_t point_size,
    std::int64_t red, std::int64_t green, std::int64_t blue) -> void;
auto sagan_5f5f72656e6465725f746578745f73637265656e(
    std::int64_t x, std::int64_t y, const std::string &value, std::int64_t point_size,
    std::int64_t red, std::int64_t green, std::int64_t blue) -> void;
