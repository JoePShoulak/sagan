#pragma once

#include "span.hpp"
#include <string>

namespace parser
{

	struct token
	{
		int id;
		span range;
		std::string text;
		double value = 0.0;
	};

} // namespace parser
