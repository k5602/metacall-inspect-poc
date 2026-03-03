/* -- Headers -- */

#include <ast_inspect/ast_cli.hpp>
#include <ast_inspect/ast_extract.hpp>
#include <ast_inspect/ast_serialize.hpp>

#include <iostream>

/* -- Methods -- */

int main(int argc, char **argv)
{
	cli_options options = {};
	analysis_result result = {};
	std::string error_message;

	if (parse_cli_options(argc, argv, options, error_message) != 0)
	{
		std::cerr << "error: " << error_message << "\n";
		return 1;
	}

	std::string language = detect_language(options.input_path);

	if (language.empty())
	{
		std::cerr << "error: unsupported extension (expected .py or .js): " << options.input_path << "\n";
		return 1;
	}

	if (extract_analysis(options.input_path, language, result, error_message) != 0)
	{
		std::cerr << "error: " << error_message << ": " << options.input_path << "\n";
		return 1;
	}

	std::string output_json = build_output_json(result, options.format, options.pretty);

	std::cout << output_json;

	if (!options.output_dir.empty())
	{
		if (write_output_json(options.input_path, options.output_dir, output_json, error_message) != 0)
		{
			std::cerr << "error: " << error_message << ": " << options.output_dir << "\n";
			return 1;
		}
	}

	return 0;
}
