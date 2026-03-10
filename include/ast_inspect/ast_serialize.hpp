#ifndef AST_INSPECT_AST_SERIALIZE_HPP
#define AST_INSPECT_AST_SERIALIZE_HPP

/* -- Headers -- */

#include <ast_inspect/ast_types.hpp>

#include <string>

/* -- Methods -- */

std::string build_output_json(const analysis_result &result, output_format_type format, bool pretty);

int write_output_json(const std::string &input_path, const std::string &output_dir, const std::string &json, std::string &error_message);

#endif /* AST_INSPECT_AST_SERIALIZE_HPP */
