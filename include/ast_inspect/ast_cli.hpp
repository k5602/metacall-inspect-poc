#ifndef AST_INSPECT_AST_CLI_HPP
#define AST_INSPECT_AST_CLI_HPP

/* -- Headers -- */

#include <ast_inspect/ast_types.hpp>

#include <string>

/* -- Methods -- */

int parse_cli_options(int argc, char **argv, cli_options &options, std::string &error_message);

#endif /* AST_INSPECT_AST_CLI_HPP */
