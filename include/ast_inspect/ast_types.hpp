#ifndef AST_INSPECT_AST_TYPES_HPP
#define AST_INSPECT_AST_TYPES_HPP

/* -- Headers -- */

#include <cstdint>
#include <string>
#include <vector>

/* -- Type Definitions -- */

enum output_format_type
{
	OUTPUT_FORMAT_AST = 0,
	OUTPUT_FORMAT_INSPECT = 1
};

typedef struct symbol_info_type
{
	std::string name;
	std::string kind;
	std::vector<std::string> args;
	uint32_t start_line;
	uint32_t end_line;
	bool exported;
} symbol_info;

typedef struct class_info_type
{
	std::string name;
	std::vector<symbol_info> constructors;
	std::vector<symbol_info> methods;
	uint32_t start_line;
	uint32_t end_line;
	bool exported;
} class_info;

typedef struct analysis_result_type
{
	std::string path;
	std::string language;
	std::string loader_tag;
	std::string module_name;
	std::vector<symbol_info> functions;
	std::vector<class_info> classes;
	std::vector<std::string> module_exports;
} analysis_result;

typedef struct cli_options_type
{
	bool pretty;
	output_format_type format;
	std::string output_dir;
	std::string input_path;
} cli_options;

#endif /* AST_INSPECT_AST_TYPES_HPP */
