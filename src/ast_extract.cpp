/* -- Headers -- */

#include <ast_inspect/ast_extract.hpp>

#include <tree_sitter/api.h>

#include <filesystem>
#include <fstream>
#include <set>

#include <cstring>

/* -- Definitions -- */

#define LANGUAGE_PYTHON		"python"
#define LANGUAGE_JAVASCRIPT "javascript"

#define LOADER_PYTHON	  "py"
#define LOADER_JAVASCRIPT "node"

#define SYMBOL_KIND_FUNCTION	"function"
#define SYMBOL_KIND_CLASS		"class"
#define SYMBOL_KIND_METHOD		"method"
#define SYMBOL_KIND_CONSTRUCTOR "constructor"

/* -- External Declarations -- */

extern "C"
{
const TSLanguage *tree_sitter_python(void);
const TSLanguage *tree_sitter_javascript(void);
}

/* -- Private Methods -- */

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

static std::string node_text(TSNode node, const char *source)
{
	if (ts_node_is_null(node))
	{
		return "";
	}

	uint32_t start = ts_node_start_byte(node);
	uint32_t end = ts_node_end_byte(node);

	if (end < start)
	{
		return "";
	}

	return std::string(source + start, end - start);
}

static TSNode child_by_field(TSNode node, const char *field_name)
{
	return ts_node_child_by_field_name(node, field_name, static_cast<uint32_t>(std::strlen(field_name)));
}

static void append_unique_name(std::vector<std::string> &names, const std::string &name)
{
	if (name.empty())
	{
		return;
	}

	for (const std::string &existing : names)
	{
		if (existing == name)
		{
			return;
		}
	}

	names.push_back(name);
}

static std::string first_identifier(TSNode node, const char *source)
{
	if (ts_node_is_null(node))
	{
		return "";
	}

	if (std::strcmp(ts_node_type(node), "identifier") == 0)
	{
		return node_text(node, source);
	}

	uint32_t child_count = ts_node_named_child_count(node);

	for (uint32_t i = 0; i < child_count; ++i)
	{
		std::string nested = first_identifier(ts_node_named_child(node, i), source);

		if (!nested.empty())
		{
			return nested;
		}
	}

	return "";
}

static std::vector<std::string> extract_python_params(TSNode parameters, const char *source)
{
	std::vector<std::string> args;
	uint32_t count = ts_node_named_child_count(parameters);

	for (uint32_t i = 0; i < count; ++i)
	{
		TSNode node = ts_node_named_child(parameters, i);
		const char *type_name = ts_node_type(node);
		std::string arg_name;

		if (std::strcmp(type_name, "identifier") == 0)
		{
			arg_name = node_text(node, source);
		}
		else if (std::strcmp(type_name, "typed_parameter") == 0 || std::strcmp(type_name, "default_parameter") == 0 || std::strcmp(type_name, "typed_default_parameter") == 0 || std::strcmp(type_name, "list_splat_pattern") == 0 || std::strcmp(type_name, "dictionary_splat_pattern") == 0)
		{
			TSNode name_node = child_by_field(node, "name");

			if (!ts_node_is_null(name_node))
			{
				arg_name = node_text(name_node, source);
			}
			else
			{
				arg_name = first_identifier(node, source);
			}
		}

		append_unique_name(args, arg_name);
	}

	return args;
}

static std::vector<std::string> extract_javascript_params(TSNode parameters, const char *source)
{
	std::vector<std::string> args;
	uint32_t count = ts_node_named_child_count(parameters);

	for (uint32_t i = 0; i < count; ++i)
	{
		TSNode node = ts_node_named_child(parameters, i);
		const char *type_name = ts_node_type(node);
		std::string arg_name;

		if (std::strcmp(type_name, "identifier") == 0)
		{
			arg_name = node_text(node, source);
		}
		else if (std::strcmp(type_name, "assignment_pattern") == 0)
		{
			arg_name = node_text(child_by_field(node, "left"), source);
		}
		else if (std::strcmp(type_name, "rest_pattern") == 0)
		{
			arg_name = node_text(child_by_field(node, "argument"), source);
		}

		append_unique_name(args, arg_name);
	}

	return args;
}

static void collect_python_exports(TSNode node, const char *source, std::set<std::string> &exports)
{
	const char *node_type = ts_node_type(node);

	if (std::strcmp(node_type, "assignment") == 0)
	{
		TSNode left = child_by_field(node, "left");
		TSNode right = child_by_field(node, "right");

		if (!ts_node_is_null(left) && node_text(left, source) == "__all__" && !ts_node_is_null(right))
		{
			uint32_t child_count = ts_node_named_child_count(right);

			for (uint32_t i = 0; i < child_count; ++i)
			{
				TSNode item = ts_node_named_child(right, i);
				const char *item_type = ts_node_type(item);

				if (std::strcmp(item_type, "string") == 0)
				{
					std::string value = node_text(item, source);

					if (value.size() >= 2 && ((value.front() == '\'' && value.back() == '\'') || (value.front() == '"' && value.back() == '"')))
					{
						exports.insert(value.substr(1, value.size() - 2));
					}
				}
			}
		}
	}

	uint32_t children = ts_node_child_count(node);

	for (uint32_t i = 0; i < children; ++i)
	{
		collect_python_exports(ts_node_child(node, i), source, exports);
	}
}

static void collect_javascript_exports(TSNode node, const char *source, std::set<std::string> &exports)
{
	const char *node_type = ts_node_type(node);

	if (std::strcmp(node_type, "assignment_expression") == 0)
	{
		TSNode left = child_by_field(node, "left");
		TSNode right = child_by_field(node, "right");

		if (!ts_node_is_null(left) && std::strcmp(ts_node_type(left), "member_expression") == 0)
		{
			TSNode object = child_by_field(left, "object");
			TSNode property = child_by_field(left, "property");
			std::string object_name = node_text(object, source);
			std::string property_name = node_text(property, source);

			if (object_name == "module" && property_name == "exports" && !ts_node_is_null(right) && std::strcmp(ts_node_type(right), "object") == 0)
			{
				uint32_t pair_count = ts_node_named_child_count(right);

				for (uint32_t i = 0; i < pair_count; ++i)
				{
					TSNode entry = ts_node_named_child(right, i);
					const char *entry_type = ts_node_type(entry);

					if (std::strcmp(entry_type, "pair") == 0)
					{
						TSNode value = child_by_field(entry, "value");
						TSNode key = child_by_field(entry, "key");
						std::string exported_name = node_text(value, source);

						if (exported_name.empty())
						{
							exported_name = node_text(key, source);
						}

						if (!exported_name.empty())
						{
							exports.insert(exported_name);
						}
					}
					else if (std::strcmp(entry_type, "shorthand_property_identifier") == 0 || std::strcmp(entry_type, "shorthand_property_identifier_pattern") == 0 || std::strcmp(entry_type, "identifier") == 0 || std::strcmp(entry_type, "property_identifier") == 0)
					{
						std::string exported_name = node_text(entry, source);

						if (!exported_name.empty())
						{
							exports.insert(exported_name);
						}
					}
				}
			}
			else if (object_name == "exports" && !property_name.empty())
			{
				exports.insert(property_name);
			}
		}
	}
	else if (std::strcmp(node_type, "export_statement") == 0)
	{
		TSNode declaration = child_by_field(node, "declaration");

		if (!ts_node_is_null(declaration))
		{
			const char *declaration_type = ts_node_type(declaration);

			if (std::strcmp(declaration_type, "function_declaration") == 0 || std::strcmp(declaration_type, "class_declaration") == 0)
			{
				exports.insert(node_text(child_by_field(declaration, "name"), source));
			}
		}
	}

	uint32_t children = ts_node_child_count(node);

	for (uint32_t i = 0; i < children; ++i)
	{
		collect_javascript_exports(ts_node_child(node, i), source, exports);
	}
}

static class_info parse_python_class(TSNode class_node, const char *source)
{
	class_info cls = {};
	TSNode name_node = child_by_field(class_node, "name");
	TSNode body_node = child_by_field(class_node, "body");
	TSPoint start_point = ts_node_start_point(class_node);
	TSPoint end_point = ts_node_end_point(class_node);

	cls.name = node_text(name_node, source);
	cls.start_line = start_point.row + 1;
	cls.end_line = end_point.row + 1;
	cls.exported = false;

	if (!ts_node_is_null(body_node))
	{
		uint32_t count = ts_node_named_child_count(body_node);

		for (uint32_t i = 0; i < count; ++i)
		{
			TSNode child = ts_node_named_child(body_node, i);

			if (std::strcmp(ts_node_type(child), "function_definition") == 0)
			{
				TSNode method_name_node = child_by_field(child, "name");
				TSNode params = child_by_field(child, "parameters");
				TSPoint method_start = ts_node_start_point(child);
				TSPoint method_end = ts_node_end_point(child);
				std::string method_name = node_text(method_name_node, source);

				symbol_info method = {
					method_name,
					(std::string(method_name) == "__init__") ? SYMBOL_KIND_CONSTRUCTOR : SYMBOL_KIND_METHOD,
					extract_python_params(params, source),
					method_start.row + 1,
					method_end.row + 1,
					false
				};

				if (method.kind == SYMBOL_KIND_CONSTRUCTOR)
				{
					cls.constructors.push_back(method);
				}
				else
				{
					cls.methods.push_back(method);
				}
			}
		}
	}

	return cls;
}

static class_info parse_javascript_class(TSNode class_node, const char *source)
{
	class_info cls = {};
	TSNode name_node = child_by_field(class_node, "name");
	TSNode body_node = child_by_field(class_node, "body");
	TSPoint start_point = ts_node_start_point(class_node);
	TSPoint end_point = ts_node_end_point(class_node);

	cls.name = node_text(name_node, source);
	cls.start_line = start_point.row + 1;
	cls.end_line = end_point.row + 1;
	cls.exported = false;

	if (!ts_node_is_null(body_node))
	{
		uint32_t count = ts_node_named_child_count(body_node);

		for (uint32_t i = 0; i < count; ++i)
		{
			TSNode child = ts_node_named_child(body_node, i);

			if (std::strcmp(ts_node_type(child), "method_definition") == 0)
			{
				TSNode method_name_node = child_by_field(child, "name");
				TSNode params = child_by_field(child, "parameters");
				TSPoint method_start = ts_node_start_point(child);
				TSPoint method_end = ts_node_end_point(child);
				std::string method_name = node_text(method_name_node, source);

				symbol_info method = {
					method_name,
					(method_name == "constructor") ? SYMBOL_KIND_CONSTRUCTOR : SYMBOL_KIND_METHOD,
					extract_javascript_params(params, source),
					method_start.row + 1,
					method_end.row + 1,
					false
				};

				if (method.kind == SYMBOL_KIND_CONSTRUCTOR)
				{
					cls.constructors.push_back(method);
				}
				else
				{
					cls.methods.push_back(method);
				}
			}
		}
	}

	return cls;
}

static void parse_python_root(TSNode root, const char *source, analysis_result &result)
{
	std::set<std::string> exports;
	collect_python_exports(root, source, exports);

	uint32_t count = ts_node_named_child_count(root);

	for (uint32_t i = 0; i < count; ++i)
	{
		TSNode node = ts_node_named_child(root, i);
		const char *type_name = ts_node_type(node);

		if (std::strcmp(type_name, "function_definition") == 0)
		{
			TSNode name_node = child_by_field(node, "name");
			TSNode params = child_by_field(node, "parameters");
			TSPoint start = ts_node_start_point(node);
			TSPoint end = ts_node_end_point(node);
			std::string name = node_text(name_node, source);

			symbol_info function_symbol = {
				name,
				SYMBOL_KIND_FUNCTION,
				extract_python_params(params, source),
				start.row + 1,
				end.row + 1,
				exports.empty() ? true : (exports.find(name) != exports.end())
			};

			result.functions.push_back(function_symbol);
		}
		else if (std::strcmp(type_name, "class_definition") == 0)
		{
			class_info cls = parse_python_class(node, source);
			cls.exported = exports.empty() ? true : (exports.find(cls.name) != exports.end());
			result.classes.push_back(cls);
		}
	}

	for (const std::string &name : exports)
	{
		result.module_exports.push_back(name);
	}
}

static void parse_javascript_root(TSNode root, const char *source, analysis_result &result)
{
	std::set<std::string> exports;
	collect_javascript_exports(root, source, exports);

	uint32_t count = ts_node_named_child_count(root);

	for (uint32_t i = 0; i < count; ++i)
	{
		TSNode node = ts_node_named_child(root, i);
		const char *type_name = ts_node_type(node);

		if (std::strcmp(type_name, "function_declaration") == 0)
		{
			TSNode name_node = child_by_field(node, "name");
			TSNode params = child_by_field(node, "parameters");
			TSPoint start = ts_node_start_point(node);
			TSPoint end = ts_node_end_point(node);
			std::string name = node_text(name_node, source);

			symbol_info function_symbol = {
				name,
				SYMBOL_KIND_FUNCTION,
				extract_javascript_params(params, source),
				start.row + 1,
				end.row + 1,
				exports.empty() ? true : (exports.find(name) != exports.end())
			};

			result.functions.push_back(function_symbol);
		}
		else if (std::strcmp(type_name, "class_declaration") == 0)
		{
			class_info cls = parse_javascript_class(node, source);
			cls.exported = exports.empty() ? true : (exports.find(cls.name) != exports.end());
			result.classes.push_back(cls);
		}
	}

	for (const std::string &name : exports)
	{
		result.module_exports.push_back(name);
	}
}

/* -- Methods -- */

std::string detect_language(const std::string &path)
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

int extract_analysis(const std::string &path, const std::string &language, analysis_result &result, std::string &error_message)
{
	std::string source = read_file_to_string(path);
	const TSLanguage *grammar = NULL;

	result.path = path;
	result.language = language;
	result.functions.clear();
	result.classes.clear();
	result.module_exports.clear();

	if (source.empty())
	{
		error_message = "cannot open or read input file";
		return 1;
	}

	std::filesystem::path input_path(path);
	result.module_name = input_path.stem().string();

	if (language == LANGUAGE_PYTHON)
	{
		grammar = tree_sitter_python();
		result.loader_tag = LOADER_PYTHON;
	}
	else
	{
		grammar = tree_sitter_javascript();
		result.loader_tag = LOADER_JAVASCRIPT;
	}

	TSParser *parser = ts_parser_new();

	if (parser == NULL)
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

	TSTree *tree = ts_parser_parse_string(parser, NULL, source.c_str(), static_cast<uint32_t>(source.size()));

	if (tree == NULL)
	{
		ts_parser_delete(parser);
		error_message = "parse failed";
		return 1;
	}

	TSNode root = ts_tree_root_node(tree);

	if (language == LANGUAGE_PYTHON)
	{
		parse_python_root(root, source.c_str(), result);
	}
	else
	{
		parse_javascript_root(root, source.c_str(), result);
	}

	ts_tree_delete(tree);
	ts_parser_delete(parser);

	return 0;
}
