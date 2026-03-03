/* -- Headers -- */

#include <tree_sitter/api.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <cstdint>
#include <cstring>

/* -- Definitions -- */

#define LANGUAGE_PYTHON		"python"
#define LANGUAGE_JAVASCRIPT "javascript"

#define SYMBOL_KIND_FUNCTION "function"
#define SYMBOL_KIND_CLASS	 "class"

/* -- Type Definitions -- */

typedef struct symbol_info_type
{
	std::string name;
	std::string kind;
	uint32_t start_line;
	uint32_t end_line;
} symbol_info;

/* -- External Declarations -- */

extern "C"
{
const TSLanguage *tree_sitter_python(void);
const TSLanguage *tree_sitter_javascript(void);
}

/* -- Private Methods -- */

static std::string detect_language(const std::string &path)
{
	auto ext = std::filesystem::path(path).extension();

	if (ext == ".py")
	{
		return LANGUAGE_PYTHON;
	}

	if (ext == ".js" || ext == ".mjs" || ext == ".cjs")
	{
		return LANGUAGE_JAVASCRIPT;
	}

	return "";
}

static std::string read_file_to_string(const std::string &path)
{
	std::ifstream file(path, std::ios::binary | std::ios::ate);

	if (!file)
	{
		return "";
	}

	std::streamsize file_size = static_cast<std::streamsize>(file.tellg());

	if (file_size < 0)
	{
		return "";
	}

	std::string source(static_cast<size_t>(file_size), '\0');

	file.seekg(0);
	file.read(source.data(), file_size);

	return source;
}

static std::string json_escape(const std::string &input)
{
	std::string output;
	output.reserve(input.size());

	for (const char ch : input)
	{
		switch (ch)
		{
			case '"':
				output += "\\\"";
				break;
			case '\\':
				output += "\\\\";
				break;
			case '\b':
				output += "\\b";
				break;
			case '\f':
				output += "\\f";
				break;
			case '\n':
				output += "\\n";
				break;
			case '\r':
				output += "\\r";
				break;
			case '\t':
				output += "\\t";
				break;
			default:
				output += ch;
				break;
		}
	}

	return output;
}

static std::string json_indent(int level)
{
	return std::string(static_cast<size_t>(level) * 2, ' ');
}

static void collect_symbols_python(TSNode node, const char *source, std::vector<symbol_info> &symbols)
{
	const char *node_type = ts_node_type(node);
	const char *kind = NULL;

	if (std::strcmp(node_type, "function_definition") == 0)
	{
		kind = SYMBOL_KIND_FUNCTION;
	}
	else if (std::strcmp(node_type, "class_definition") == 0)
	{
		kind = SYMBOL_KIND_CLASS;
	}

	if (kind != NULL)
	{
		TSNode name_node = ts_node_child_by_field_name(node, "name", 4);

		if (!ts_node_is_null(name_node))
		{
			uint32_t start = ts_node_start_byte(name_node);
			uint32_t end = ts_node_end_byte(name_node);
			TSPoint start_point = ts_node_start_point(node);
			TSPoint end_point = ts_node_end_point(node);

			symbol_info symbol = {
				std::string(source + start, end - start),
				kind,
				start_point.row + 1,
				end_point.row + 1
			};

			symbols.push_back(symbol);
		}
	}

	/* Depth-first CST traversal keeps this parser-independent and avoids query setup overhead for the PoC. */
	uint32_t child_count = ts_node_child_count(node);

	for (uint32_t i = 0; i < child_count; ++i)
	{
		collect_symbols_python(ts_node_child(node, i), source, symbols);
	}
}

static void collect_symbols_javascript(TSNode node, const char *source, std::vector<symbol_info> &symbols)
{
	const char *node_type = ts_node_type(node);
	const char *kind = NULL;

	if (std::strcmp(node_type, "function_declaration") == 0)
	{
		kind = SYMBOL_KIND_FUNCTION;
	}
	else if (std::strcmp(node_type, "class_declaration") == 0)
	{
		kind = SYMBOL_KIND_CLASS;
	}

	if (kind != NULL)
	{
		TSNode name_node = ts_node_child_by_field_name(node, "name", 4);

		if (!ts_node_is_null(name_node))
		{
			uint32_t start = ts_node_start_byte(name_node);
			uint32_t end = ts_node_end_byte(name_node);
			TSPoint start_point = ts_node_start_point(node);
			TSPoint end_point = ts_node_end_point(node);

			symbol_info symbol = {
				std::string(source + start, end - start),
				kind,
				start_point.row + 1,
				end_point.row + 1
			};

			symbols.push_back(symbol);
		}
	}

	/* Depth-first CST traversal keeps this parser-independent and avoids query setup overhead for the PoC. */
	uint32_t child_count = ts_node_child_count(node);

	for (uint32_t i = 0; i < child_count; ++i)
	{
		collect_symbols_javascript(ts_node_child(node, i), source, symbols);
	}
}

static int extract_symbols(
	const std::string &path,
	const std::string &language,
	std::vector<symbol_info> &symbols,
	std::string &error_message)
{
	std::string source = read_file_to_string(path);

	if (source.empty())
	{
		error_message = "cannot open or read input file";
		return 1;
	}

	const TSLanguage *grammar = nullptr;

	if (language == LANGUAGE_PYTHON)
	{
		grammar = tree_sitter_python();
	}
	else
	{
		grammar = tree_sitter_javascript();
	}

	TSParser *parser = ts_parser_new();

	if (parser == nullptr)
	{
		error_message = "parser allocation failed";
		return 1;
	}

	if (!ts_parser_set_language(parser, grammar))
	{
		ts_parser_delete(parser);
		error_message = "failed to set parser language";
		return 1;
	}

	TSTree *tree = ts_parser_parse_string(parser, nullptr, source.c_str(), static_cast<uint32_t>(source.size()));

	if (tree == nullptr)
	{
		ts_parser_delete(parser);
		error_message = "parse failed";
		return 1;
	}

	TSNode root = ts_tree_root_node(tree);

	if (language == LANGUAGE_PYTHON)
	{
		collect_symbols_python(root, source.c_str(), symbols);
	}
	else
	{
		collect_symbols_javascript(root, source.c_str(), symbols);
	}

	ts_tree_delete(tree);
	ts_parser_delete(parser);

	return 0;
}

static std::string build_symbols_json(const std::string &path, const std::string &language, const std::vector<symbol_info> &symbols, bool pretty)
{
	std::ostringstream output;
	const std::string newline = pretty ? "\n" : "";
	const std::string separator = pretty ? " " : "";

	output << "{" << newline;

	if (pretty)
	{
		output << json_indent(1);
	}

	output << "\"path\":" << separator << "\"" << json_escape(path) << "\"," << newline;

	if (pretty)
	{
		output << json_indent(1);
	}

	output << "\"language\":" << separator << "\"" << json_escape(language) << "\"," << newline;

	if (pretty)
	{
		output << json_indent(1);
	}

	output << "\"symbols\":" << separator << "[";

	if (pretty && !symbols.empty())
	{
		output << newline;
	}

	for (size_t i = 0; i < symbols.size(); ++i)
	{
		const symbol_info &symbol = symbols[i];

		if (pretty)
		{
			output << json_indent(2);
		}

		output << "{";

		if (pretty)
		{
			output << newline << json_indent(3);
		}

		output << "\"name\":" << separator << "\"" << json_escape(symbol.name) << "\",";

		if (pretty)
		{
			output << newline << json_indent(3);
		}

		output << "\"kind\":" << separator << "\"" << json_escape(symbol.kind) << "\",";

		if (pretty)
		{
			output << newline << json_indent(3);
		}

		output << "\"start_line\":" << separator << symbol.start_line << ",";

		if (pretty)
		{
			output << newline << json_indent(3);
		}

		output << "\"end_line\":" << separator << symbol.end_line;

		if (pretty)
		{
			output << newline << json_indent(2);
		}

		output << "}";

		if (i + 1 < symbols.size())
		{
			output << ",";
		}

		if (pretty)
		{
			output << newline;
		}
	}

	if (pretty)
	{
		output << json_indent(1);
	}

	output << "]" << newline << "}" << newline;

	return output.str();
}

static int write_output_json(
	const std::string &input_path,
	const std::string &output_dir,
	const std::string &json,
	std::string &error_message)
{
	std::filesystem::path output_path(output_dir);
	std::error_code error;

	std::filesystem::create_directories(output_path, error);

	if (error)
	{
		error_message = "failed to create output directory";
		return 1;
	}

	std::filesystem::path input_file(input_path);
	std::string output_file_name = input_file.filename().string() + ".json";

	output_path /= output_file_name;

	std::ofstream output_file(output_path, std::ios::binary | std::ios::trunc);

	if (!output_file)
	{
		error_message = "failed to open output file";
		return 1;
	}

	/* The JSON string lifetime is owned by the caller; write it atomically to avoid partial output states. */
	output_file.write(json.c_str(), static_cast<std::streamsize>(json.size()));

	if (!output_file.good())
	{
		error_message = "failed to write output file";
		return 1;
	}

	return 0;
}

/* -- Methods -- */

int main(int argc, char **argv)
{
	if (argc < 2)
	{
		std::cerr << "usage: ast_inspect [--pretty] [--output-dir <dir>] <file.py|file.js>\n";
		return 1;
	}

	bool pretty = false;
	std::string output_dir;
	std::string path;

	for (int i = 1; i < argc; ++i)
	{
		std::string arg = argv[i];

		if (arg == "--pretty")
		{
			pretty = true;
			continue;
		}

		if (arg == "--output-dir")
		{
			if (i + 1 >= argc)
			{
				std::cerr << "error: --output-dir requires a directory argument\n";
				return 1;
			}

			output_dir = argv[++i];
			continue;
		}

		if (!arg.empty() && arg[0] == '-')
		{
			std::cerr << "error: unknown option: " << arg << "\n";
			return 1;
		}

		if (!path.empty())
		{
			std::cerr << "error: only one input file is supported\n";
			return 1;
		}

		path = arg;
	}

	if (path.empty())
	{
		std::cerr << "error: missing input file path\n";
		std::cerr << "usage: ast_inspect [--pretty] [--output-dir <dir>] <file.py|file.js>\n";
		return 1;
	}

	std::string lang = detect_language(path);

	if (lang.empty())
	{
		std::cerr << "unsupported extension (expected .py or .js): " << path << "\n";
		return 1;
	}

	std::vector<symbol_info> symbols;
	std::string error_message;

	if (extract_symbols(path, lang, symbols, error_message) != 0)
	{
		std::cerr << "error: " << error_message << ": " << path << "\n";
		return 1;
	}

	std::string json = build_symbols_json(path, lang, symbols, pretty);
	std::cout << json;

	if (!output_dir.empty())
	{
		if (write_output_json(path, output_dir, json, error_message) != 0)
		{
			std::cerr << "error: " << error_message << ": " << output_dir << "\n";
			return 1;
		}
	}

	return 0;
}
