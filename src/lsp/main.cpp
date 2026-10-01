#include "server.hpp"

#include <iostream>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

auto main() -> int
{
#ifdef _WIN32
  _setmode(_fileno(stdin), _O_BINARY);
  _setmode(_fileno(stdout), _O_BINARY);
#endif
  sagan::lsp::server service;
  return sagan::lsp::run(std::cin, std::cout, service);
}
