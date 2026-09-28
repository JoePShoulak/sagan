#pragma once

#include "span.hpp"
#include <exception>
#include <string>

namespace parser
{

	struct parse_error : public std::exception
	{
		const std::string message;
		const span range;

		parse_error(const std::string message, span range);

		const char *what() const noexcept override;
	};

} // namespace parser
