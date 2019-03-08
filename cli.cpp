#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

#include "base.h"
#include "cli.h"
#include "templates.h"
#include "string.h"

#define is_alpha(c) ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'))
#define is_identchr(c) (is_alpha(c) || (c >= '0' && c <= '9') || c == '-' || c == '_')

#define MAX_POS_ARGS 10

static void
print_usage(char *exec_name) {
    printf("Usage:\n");
    printf("%s [flags] <action name> | --template <template string> [variables]\n", exec_name);
}

static void
print_help() {
    printf("\n");
    printf("qs (quick-scripts): A tiny utility for keeping a catalogue of one-liners.\n");
    printf("\n");
    printf("Scripts are defined using a trivial key=value binding of an <action name>=<template>.\n");
    printf("The templates may contain variables which are substituted using arguments given to qs.\n");
    printf("\n");
    printf("Usage:\n");
    printf("qs <action name> [options] [variables]\n");
    printf("Options:\n");
    printf("  --dry-run:  Just print the command that would have run, don't actually run it.\n");
    printf("  --verbose:  Print more information while executing.\n");
    printf("  --template: Ignore the preconfigured templates and use an explicit template instead. Cannot be used together with <action name>\n");
    printf("  --version:  Print the current version and exit.\n");
    printf("\n");
    printf("Any other --<argument> is interpreted as a variable substitution to use for the resolved template.\n");
    printf("\n");
    printf("Action name is a preconfigured action name in one of the resolved config files (in order).\n");
    printf(" - `.qs.cfg` file in the current directory\n");
    printf(" - `.qs.cfg` file in a parent git source-root of the current directory\n");
    printf(" - `$XDG_CONFIG_HOME/qs/default.cfg`\n");
    printf(" - `$HOME/.configs/qs/default.cfg` (unless XDG_CONFIG_HOME is set)\n");
    printf("\n");
}

static bool
is_identifier(const char * val) {
    u32 i = cstrlen(val);
    while (i--) {
        if ((i == 0 && !is_alpha(val[i])) || (i > 0 && !is_identchr(val[i]))) {
            return false;
        }
    }
    return true;
}

ParseResult
parse_cli_args(CommandLineOptions *options, int num_args, char ** args) {
    if (num_args < 1)
    {
        return ParseResult_Invalid;
    } else if (num_args < 2) {
        // Need at least one argument
        print_usage(args[0]);
        return ParseResult_Invalid;
    }

    u8 num_pos_args = 0;
    int arg_index = 1; // Skip binary name
    char * current_arg = args[arg_index];
    while (arg_index < num_args)
    {
        // TODO(christoffer) Parse -d commands as arguments
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

                if (char * resolved_path = realpath(current_arg, 0)) {
                    options->config_files = string_push_dup_front(options->config_files, resolved_path);
                    free(resolved_path);
                } else {
                    fprintf(stdout, "Warning: could not read the config file '%s'. Ignoring.", current_arg);
                }
            } else if (string_eq(current_arg, "--help")) {
                if (options->action_name) {
                    // --help came after the action name. Set the flag for displaying the auto-generated
                    // help string for the command (done when parsing the template).
                    options->print_action_help = true;
                } else {
                    print_help();
                    return ParseResult_Stop;
                }
            } else if (string_eq(current_arg, "--version")) {
                fprintf(stdout, "%s\n", QUICK_SCRIPT_VERSION);
                return ParseResult_Stop;
            } else if (string_eq(current_arg, "--template")) {
                // Make sure we have a template value
                if (++arg_index < num_args) {
                    current_arg = args[arg_index];
                } else {
                    fprintf(stdout, "--template should be followed by a template string.\n");
                    return ParseResult_Invalid;
                }

                // Set or overwrite the template string
                if (options->template_string) {
                    options->template_string = string_copy(options->template_string, current_arg);
                } else {
                    options->template_string = string_new(current_arg);
                }
            } else {
                /*** Parse as named varible ***/

                char * varname = (current_arg + 2); // skip '--'
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
            char varname[2] = {(char)('0' + num_pos_args++), 0};
            options->variables = template_set(options->variables, varname, args[arg_index]);
        } else {
            // Until we have a valid action name, we expect anything else to be the action name.
            // If it's not a valid identifier, treat it as an error.
            if (!is_identifier(current_arg)) {
                fprintf(stdout, "'%s' is not a valid action name. Action names must start with a letter, followed by letters, numbers, a a dash (-) or an underscore (_)\n", current_arg);
                return ParseResult_Invalid;
            }
            options->action_name = string_new(current_arg);
        }
        current_arg = args[++arg_index];
    }

    if (options->action_name && options->template_string) {
        fprintf(stdout, "Error: Must provide either an action name or a template string (--template), not both.\n");
        return ParseResult_Invalid;
    }

    if (!options->action_name && !options->template_string) {
        fprintf(stdout, "Must provide either an action name or a --template\n");
        return ParseResult_Invalid;
    }

    return ParseResult_Procceed;
}

void
free_cli_options_resources(CommandLineOptions options) {
    if (options.action_name) string_free(options.action_name);
    if (options.template_string) string_free(options.template_string);
    StringList * node = options.config_files;
    while (node) {
        StringList * next = node->next;
        if (node->string) string_free(node->string);
        free(node);
        node = next;
    }
    if (options.variables) {
        template_free(options.variables);
    }
}
