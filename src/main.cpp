#include "parser/tokenizer.hpp"
#include <regex>
#include "parser/span.hpp"
#include <iostream>
#include "parser/parse_error.hpp"
#include "parser/tokens.hpp"
#include "parser/lex.hpp"

int main()
{
  parser::tokenizer sagan({"{}", 0}, get_token);

  for (auto token : sagan)
  {
    std::cout << token.text << std::endl;
  }
}
