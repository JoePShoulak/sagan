#include "parse_error.hpp"

namespace parser
{

	parse_error::parse_error(const std::string message, span range) : message(message), range(range) {}

	const char *parse_error::what() const noexcept
	{
		return message.c_str();
	}

} // namespace parser
