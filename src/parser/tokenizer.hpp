#pragma once

#include "token.hpp"
#include "generator.hpp"
#include <string>

namespace parser
{

	struct programText
	{
		std::string text;
		int index;
	};

	typedef z::core::generator<token, programText> _tokenizer;

	class tokenizer : public _tokenizer
	{
		bool token_pulled = false;
		int prev_index = 0;
		std::optional<token> tok;

	public:
		using _tokenizer::_tokenizer;

		auto next() -> std::optional<token> override;

		auto get_span() -> span;
		auto existing_token() -> std::optional<token>;

		auto get_token() -> std::optional<token>;
		auto started() const -> bool;
		auto empty() const -> bool;
	};

} // namespace parser
