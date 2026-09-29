#pragma once

#include <string>

namespace driver
{
  auto compile_and_run(const std::string &generated_cpp) -> int;
}
