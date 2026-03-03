/* -- Headers -- */

#include <ast_inspect/ast_cli.hpp>

/* -- Methods -- */

int parse_cli_options(int argc, char **argv, cli_options &options, std::string &error_message)
{
	options.pretty = false;
	options.format = OUTPUT_FORMAT_AST;
	options.output_dir.clear();
	options.input_path.clear();

	if (argc < 2)
	{
		error_message = "usage: ast_inspect [--pretty] [--format ast|inspect] [--output-dir <dir>] <file.py|file.js>";
		return 1;
	}

	for (int i = 1; i < argc; ++i)
	{
		std::string arg = argv[i];

		if (arg == "--pretty")
		{
			options.pretty = true;
			continue;
		}

		if (arg == "--format")
		{
			if (i + 1 >= argc)
			{
				error_message = "--format requires ast or inspect";
				return 1;
			}

			std::string format = argv[++i];

			if (format == "ast")
			{
				options.format = OUTPUT_FORMAT_AST;
			}
			else if (format == "inspect")
			{
				options.format = OUTPUT_FORMAT_INSPECT;
			}
			else
			{
				error_message = "invalid --format value: " + format;
				return 1;
			}

			continue;
		}

		if (arg == "--output-dir")
		{
			if (i + 1 >= argc)
			{
				error_message = "--output-dir requires a directory argument";
				return 1;
			}

			options.output_dir = argv[++i];
			continue;
		}

		if (!arg.empty() && arg[0] == '-')
		{
			error_message = "unknown option: " + arg;
			return 1;
		}

		if (!options.input_path.empty())
		{
			error_message = "only one input file is supported";
			return 1;
		}

		options.input_path = arg;
	}

	if (options.input_path.empty())
	{
		error_message = "missing input file path";
		return 1;
	}

	return 0;
}
