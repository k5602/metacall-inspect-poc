/* -- Headers -- */

#include <ast_inspect/ast_serialize.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>

/* -- Definitions -- */

#define UNKNOWN_TYPE_ID 15

/* -- Private Methods -- */

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

static void append_json_key(std::ostringstream &output, const std::string &key, bool pretty, int indent)
{
	if (pretty)
	{
		output << json_indent(indent);
	}

	output << "\"" << json_escape(key) << "\":";

	if (pretty)
	{
		output << " ";
	}
}

static void append_unknown_type(std::ostringstream &output, bool pretty, int indent)
{
	const std::string nl = pretty ? "\n" : "";

	output << "{" << nl;
	append_json_key(output, "name", pretty, indent + 1);
	output << "\"\"," << nl;
	append_json_key(output, "id", pretty, indent + 1);
	output << UNKNOWN_TYPE_ID << nl;

	if (pretty)
	{
		output << json_indent(indent);
	}

	output << "}";
}

static void append_signature(std::ostringstream &output, const std::vector<std::string> &args, bool pretty, int indent)
{
	const std::string nl = pretty ? "\n" : "";

	output << "{" << nl;
	append_json_key(output, "ret", pretty, indent + 1);
	output << "{" << nl;
	append_json_key(output, "type", pretty, indent + 2);
	append_unknown_type(output, pretty, indent + 2);
	output << nl;

	if (pretty)
	{
		output << json_indent(indent + 1);
	}

	output << "}," << nl;
	append_json_key(output, "args", pretty, indent + 1);
	output << "[";

	if (pretty && !args.empty())
	{
		output << nl;
	}

	for (size_t i = 0; i < args.size(); ++i)
	{
		if (pretty)
		{
			output << json_indent(indent + 2);
		}

		output << "{" << nl;
		append_json_key(output, "name", pretty, indent + 3);
		output << "\"" << json_escape(args[i]) << "\"," << nl;
		append_json_key(output, "type", pretty, indent + 3);
		append_unknown_type(output, pretty, indent + 3);
		output << nl;

		if (pretty)
		{
			output << json_indent(indent + 2);
		}

		output << "}";

		if (i + 1 < args.size())
		{
			output << ",";
		}

		if (pretty)
		{
			output << nl;
		}
	}

	if (pretty)
	{
		output << json_indent(indent + 1);
	}

	output << "]" << nl;

	if (pretty)
	{
		output << json_indent(indent);
	}

	output << "}";
}

static void append_signature_constructor(std::ostringstream &output, const std::vector<std::string> &args, bool pretty, int indent)
{
	const std::string nl = pretty ? "\n" : "";

	output << "{" << nl;
	append_json_key(output, "visibility", pretty, indent + 1);
	output << "\"public\"," << nl;
	append_json_key(output, "args", pretty, indent + 1);
	output << "[";

	if (pretty && !args.empty())
	{
		output << nl;
	}

	for (size_t i = 0; i < args.size(); ++i)
	{
		if (pretty)
		{
			output << json_indent(indent + 2);
		}

		output << "{" << nl;
		append_json_key(output, "name", pretty, indent + 3);
		output << "\"" << json_escape(args[i]) << "\"," << nl;
		append_json_key(output, "type", pretty, indent + 3);
		append_unknown_type(output, pretty, indent + 3);
		output << nl;

		if (pretty)
		{
			output << json_indent(indent + 2);
		}

		output << "}";

		if (i + 1 < args.size())
		{
			output << ",";
		}

		if (pretty)
		{
			output << nl;
		}
	}

	if (pretty)
	{
		output << json_indent(indent + 1);
	}

	output << "]" << nl;

	if (pretty)
	{
		output << json_indent(indent);
	}

	output << "}";
}

static std::string build_ast_json(const analysis_result &result, bool pretty)
{
	std::ostringstream output;
	const std::string nl = pretty ? "\n" : "";

	output << "{" << nl;
	append_json_key(output, "path", pretty, 1);
	output << "\"" << json_escape(result.path) << "\"," << nl;
	append_json_key(output, "language", pretty, 1);
	output << "\"" << json_escape(result.language) << "\"," << nl;
	append_json_key(output, "module_exports", pretty, 1);
	output << "[";

	for (size_t i = 0; i < result.module_exports.size(); ++i)
	{
		if (i > 0)
		{
			output << ",";
		}

		output << "\"" << json_escape(result.module_exports[i]) << "\"";
	}

	output << "]," << nl;
	append_json_key(output, "symbols", pretty, 1);
	output << "[";

	std::vector<symbol_info> flat_symbols;

	for (const symbol_info &function_symbol : result.functions)
	{
		flat_symbols.push_back(function_symbol);
	}

	for (const class_info &class_symbol : result.classes)
	{
		symbol_info as_symbol = {
			class_symbol.name,
			"class",
			{},
			class_symbol.start_line,
			class_symbol.end_line,
			class_symbol.exported
		};

		flat_symbols.push_back(as_symbol);

		for (const symbol_info &method_symbol : class_symbol.methods)
		{
			flat_symbols.push_back(method_symbol);
		}
	}

	if (pretty && !flat_symbols.empty())
	{
		output << nl;
	}

	for (size_t i = 0; i < flat_symbols.size(); ++i)
	{
		const symbol_info &symbol = flat_symbols[i];

		if (pretty)
		{
			output << json_indent(2);
		}

		output << "{" << nl;
		append_json_key(output, "name", pretty, 3);
		output << "\"" << json_escape(symbol.name) << "\"," << nl;
		append_json_key(output, "kind", pretty, 3);
		output << "\"" << json_escape(symbol.kind) << "\"," << nl;
		append_json_key(output, "start_line", pretty, 3);
		output << symbol.start_line << "," << nl;
		append_json_key(output, "end_line", pretty, 3);
		output << symbol.end_line << "," << nl;
		append_json_key(output, "exported", pretty, 3);
		output << (symbol.exported ? "true" : "false") << nl;

		if (pretty)
		{
			output << json_indent(2);
		}

		output << "}";

		if (i + 1 < flat_symbols.size())
		{
			output << ",";
		}

		if (pretty)
		{
			output << nl;
		}
	}

	if (pretty)
	{
		output << json_indent(1);
	}

	output << "]" << nl << "}" << nl;

	return output.str();
}

static std::string build_inspect_json(const analysis_result &result, bool pretty)
{
	std::ostringstream output;
	const std::string nl = pretty ? "\n" : "";

	output << "{" << nl;
	append_json_key(output, result.loader_tag, pretty, 1);
	output << "[" << nl;

	if (pretty)
	{
		output << json_indent(2);
	}

	output << "{" << nl;
	append_json_key(output, "name", pretty, 3);
	output << "\"" << json_escape(result.module_name) << "\"," << nl;
	append_json_key(output, "scope", pretty, 3);
	output << "{" << nl;
	append_json_key(output, "name", pretty, 4);
	output << "\"global_namespace\"," << nl;

	append_json_key(output, "funcs", pretty, 4);
	output << "[";

	std::vector<symbol_info> functions;
	bool exported_filter = !result.module_exports.empty();

	for (const symbol_info &function_symbol : result.functions)
	{
		if (!exported_filter || function_symbol.exported)
		{
			functions.push_back(function_symbol);
		}
	}

	if (pretty && !functions.empty())
	{
		output << nl;
	}

	for (size_t i = 0; i < functions.size(); ++i)
	{
		if (pretty)
		{
			output << json_indent(5);
		}

		output << "{" << nl;
		append_json_key(output, "name", pretty, 6);
		output << "\"" << json_escape(functions[i].name) << "\"," << nl;
		append_json_key(output, "signature", pretty, 6);
		append_signature(output, functions[i].args, pretty, 6);
		output << "," << nl;
		append_json_key(output, "async", pretty, 6);
		output << "false" << nl;

		if (pretty)
		{
			output << json_indent(5);
		}

		output << "}";

		if (i + 1 < functions.size())
		{
			output << ",";
		}

		if (pretty)
		{
			output << nl;
		}
	}

	if (pretty)
	{
		output << json_indent(4);
	}

	output << "]," << nl;

	append_json_key(output, "classes", pretty, 4);
	output << "[";

	std::vector<class_info> classes;

	for (const class_info &class_symbol : result.classes)
	{
		if (!exported_filter || class_symbol.exported)
		{
			classes.push_back(class_symbol);
		}
	}

	if (pretty && !classes.empty())
	{
		output << nl;
	}

	for (size_t i = 0; i < classes.size(); ++i)
	{
		const class_info &cls = classes[i];

		if (pretty)
		{
			output << json_indent(5);
		}

		output << "{" << nl;
		append_json_key(output, "name", pretty, 6);
		output << "\"" << json_escape(cls.name) << "\"," << nl;

		append_json_key(output, "constructors", pretty, 6);
		output << "[";

		if (pretty && !cls.constructors.empty())
		{
			output << nl;
		}

		for (size_t j = 0; j < cls.constructors.size(); ++j)
		{
			if (pretty)
			{
				output << json_indent(7);
			}

			append_signature_constructor(output, cls.constructors[j].args, pretty, 7);

			if (j + 1 < cls.constructors.size())
			{
				output << ",";
			}

			if (pretty)
			{
				output << nl;
			}
		}

		if (pretty)
		{
			output << json_indent(6);
		}

		output << "]," << nl;

		append_json_key(output, "methods", pretty, 6);
		output << "[";

		if (pretty && !cls.methods.empty())
		{
			output << nl;
		}

		for (size_t j = 0; j < cls.methods.size(); ++j)
		{
			if (pretty)
			{
				output << json_indent(7);
			}

			output << "{" << nl;
			append_json_key(output, "name", pretty, 8);
			output << "\"" << json_escape(cls.methods[j].name) << "\"," << nl;
			append_json_key(output, "signature", pretty, 8);
			append_signature(output, cls.methods[j].args, pretty, 8);
			output << "," << nl;
			append_json_key(output, "async", pretty, 8);
			output << "false," << nl;
			append_json_key(output, "visibility", pretty, 8);
			output << "\"public\"" << nl;

			if (pretty)
			{
				output << json_indent(7);
			}

			output << "}";

			if (j + 1 < cls.methods.size())
			{
				output << ",";
			}

			if (pretty)
			{
				output << nl;
			}
		}

		if (pretty)
		{
			output << json_indent(6);
		}

		output << "]," << nl;

		append_json_key(output, "static_methods", pretty, 6);
		output << "[]," << nl;
		append_json_key(output, "attributes", pretty, 6);
		output << "[]," << nl;
		append_json_key(output, "static_attributes", pretty, 6);
		output << "[]" << nl;

		if (pretty)
		{
			output << json_indent(5);
		}

		output << "}";

		if (i + 1 < classes.size())
		{
			output << ",";
		}

		if (pretty)
		{
			output << nl;
		}
	}

	if (pretty)
	{
		output << json_indent(4);
	}

	output << "]," << nl;
	append_json_key(output, "objects", pretty, 4);
	output << "[]" << nl;

	if (pretty)
	{
		output << json_indent(3);
	}

	output << "}" << nl;

	if (pretty)
	{
		output << json_indent(2);
	}

	output << "}" << nl;

	if (pretty)
	{
		output << json_indent(1);
	}

	output << "]" << nl << "}" << nl;

	return output.str();
}

/* -- Methods -- */

std::string build_output_json(const analysis_result &result, output_format_type format, bool pretty)
{
	if (format == OUTPUT_FORMAT_INSPECT)
	{
		return build_inspect_json(result, pretty);
	}

	return build_ast_json(result, pretty);
}

int write_output_json(const std::string &input_path, const std::string &output_dir, const std::string &json, std::string &error_message)
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

	output_file.write(json.c_str(), static_cast<std::streamsize>(json.size()));

	if (!output_file.good())
	{
		error_message = "failed to write output file";
		return 1;
	}

	return 0;
}
