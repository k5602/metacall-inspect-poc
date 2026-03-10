#ifndef AST_INSPECT_AST_EXTRACT_HPP
#define AST_INSPECT_AST_EXTRACT_HPP

/* -- Headers -- */

#include <ast_inspect/ast_types.hpp>

#include <string>

/* -- Methods -- */

std::string detect_language(const std::string &path);

int extract_analysis(const std::string &path, const std::string &language, analysis_result &result, std::string &error_message);

#endif /* AST_INSPECT_AST_EXTRACT_HPP */
