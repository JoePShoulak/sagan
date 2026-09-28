#pragma once

#include "token.hpp"
#include "generator.hpp"
#include <deque>
#include <string>
#include <utility>
#include <vector>

namespace parser
{

	struct programText
	{
		struct string_context
		{
			bool multiline;
			char quote;
			int open_index;
		};

		std::string text;
		int index;
		std::deque<token> pending;
		bool in_string = false;
		bool string_raw = false;
		bool string_multiline = false;
		bool string_interpolated = false;
		char string_quote = '\0';
		int string_open_index = 0;
		int interpolation_depth = 0;
		std::vector<string_context> string_stack;
		bool line_has_token = false;
		bool emitted_final_newline = false;
		bool source_validated = false;
		bool newline_continuation = false;
		int parenthesis_depth = 0;
		int bracket_depth = 0;

		explicit programText(std::string source) : text(std::move(source)), index(0) {}
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
