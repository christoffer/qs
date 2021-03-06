#include <assert.h>
#include <limits.h>
#include <stdio.h>

#include "cli.h"

#define is_alpha(c) ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'))
#define is_identchr(c) (is_alpha(c) || (c >= '0' && c <= '9') || c == '-' || c == '_')

#define MAX_POS_ARGS 10

static bool
is_identifier(const char* val)
{
    u32 i = cstrlen(val);
    while (i--) {
        if ((i == 0 && !is_alpha(val[i])) || (i > 0 && !is_identchr(val[i]))) {
            return false;
        }
    }
    return true;
}

ParseResult
parse_cli_args(CommandLineOptions* options, int num_args, char** args)
{
    if (num_args < 1) {
        return ParseResult_Invalid;
    } else if (num_args < 2) {
        // Need at least one argument
        options->no_arguments_given = true;
        return ParseResult_Ok;
    }

    u8 num_pos_args = 0;
    int arg_index = 1; // Skip binary name
    char* current_arg = args[arg_index];
    while (arg_index < num_args) {
        // Parse --arguments
        if (string_starts_with(current_arg, "--")) {
            /*** Parse option flags for qs itself ***/

            if (string_eq(current_arg, "--dry-run")) {
                options->dry_run = true;
            } else if (string_eq(current_arg, "--verbose")) {
                options->verbose = true;
            } else if (string_eq(current_arg, "--config")) {
                // Make sure we have a path value
                if (++arg_index < num_args) {
                    current_arg = args[arg_index];
                } else {
                    fprintf(stdout, "Argument --config should be followed by a file path.\n");
                    return ParseResult_Invalid;
                }

                if (char* resolved_path = realpath(current_arg, 0)) {
                    options->config_files = string_list_add_front_dup(options->config_files, resolved_path);
                    free(resolved_path);
                } else {
                    fprintf(stdout, "Warning: could not read the config file '%s'. Ignoring.\n", current_arg);
                }
            } else if (string_eq(current_arg, "--help")) {
                if (options->action_name) {
                    // --help came after the action name. Set the flag for displaying the auto-generated
                    // help string for the command (done when parsing the template).
                    options->print_action_help = true;
                    // Don't stop here, because we need to collect any additional --config files.
                } else {
                    options->print_help = true;
                    // Exit since printing the help is exclusive
                    return ParseResult_Ok;
                }
            } else if (string_eq(current_arg, "--version")) {
                options->print_version = true;
                // Exit since printing the version is exclusive
                return ParseResult_Ok;
            } else if (string_eq(current_arg, "--template")) {
                // Make sure we have a template value
                if (++arg_index < num_args) {
                    current_arg = args[arg_index];
                } else {
                    fprintf(stdout, "--template should be followed by a template string.\n");
                    return ParseResult_Invalid;
                }

                // Set or overwrite the template string
                if (options->action_template) {
                    options->action_template = string_copy(options->action_template, current_arg);
                } else {
                    options->action_template = string_new(current_arg);
                }
            } else if (string_eq(current_arg, "--actions")) {
                options->print_available_actions = true;
            } else {
                /*** Parse as named varible ***/

                char* varname = (current_arg + 2); // skip '--'
                if (!is_identifier(varname)) {
                    fprintf(stdout, "Variable name '%s' is not a valid name. Variables must start with a letter, and consist only of letters, numbers and '-' and '_' (e.g. --some-variable_1, --NAME1).\n", varname);
                    return ParseResult_Invalid;
                }

                // Advance one argument to get the value
                if (++arg_index < num_args) {
                    options->variables = template_set(options->variables, varname, args[arg_index]);
                } else {
                    fprintf(stdout, "Missing value for variable '%s'\n", varname);
                    return ParseResult_Invalid;
                }
            }
        } else if (options->action_name) {
            // We've got an action name, and have already checked for any other known argument.
            // Treat this as a positional argument.
            if (num_pos_args >= MAX_POS_ARGS) {
                fprintf(stdout, "At most %d positional arguments can be given. Wrap arguments containing spaces in double quotes (\").\n", MAX_POS_ARGS);
                return ParseResult_Invalid;
            }
            char varname[2] = { (char)('0' + num_pos_args++), 0 };
            options->variables = template_set(options->variables, varname, args[arg_index]);
        } else {
            //
            // If it's not a valid identifier, treat it as an error.
            if (!is_identifier(current_arg)) {
                fprintf(stdout, "'%s' is not a valid action name. Action names must start with a letter, followed by letters, numbers, a a dash (-) or an underscore (_)\n", current_arg);
                return ParseResult_Invalid;
            }
            options->action_name = string_new(current_arg);
        }
        current_arg = args[++arg_index];
    }

    return ParseResult_Ok;
}

void free_cli_options_resources(CommandLineOptions options)
{
    if (options.action_name)
        string_free(options.action_name);
    if (options.action_template)
        string_free(options.action_template);
    string_list_free(options.config_files);
    template_free(options.variables);
}
