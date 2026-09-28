#pragma once

#include "span.hpp"
#include <istream>
#include <string>

namespace parser
{

	struct context
	{
		span line;
		span col;
		std::string text;

		context(std::istream &stream, span range);
	};

} // namespace parser
