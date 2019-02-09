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

static char *
get_arg(char ** args, int arg_index, int num_args) {
    if (arg_index < num_args) {
        return args[arg_index];
    }
    return NULL;
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

    // Grab the current working directory so that we can resolve relative paths
    // to config files, if needed.
    char cwd[PATH_MAX] = {0};
    if (!realpath(".", cwd))
    {
        fprintf(stderr, "Error: Could not resolve the current directory.\n");
        return ParseResult_Error;
    }

    int arg_index = 1; // Skip binary name
    char * current_arg = get_arg(args, arg_index, num_args);
    while (current_arg)
    {
        // TODO(christoffer) Parse -d commands as arguments
        // Parse --arguments
        if (string_starts_with(current_arg, "--")) {
            current_arg += 2; // skip leading '--'
            if (string_eq(current_arg, "dry-run")) {
                options->dry_run = true;
            } else if (string_eq(current_arg, "verbose")) {
                options->verbose = true;
            } else if (string_eq(current_arg, "config")) {
                current_arg = get_arg(args, ++arg_index, num_args);
                if (current_arg == NULL) {
                    fprintf(stdout, "Argument --config should be followed by a file path.\n");
                    return ParseResult_Invalid;
                }
                char resolved_path[PATH_MAX] = {0};
                if (realpath(current_arg, resolved_path)) {
                    StringList * node = (StringList *) calloc(1, sizeof(StringList));
                    node->string = string_new(resolved_path);
                    node->next = options->config_files;
                    options->config_files = node;
                } else {
                    fprintf(stdout, "Warning: could not read the config file '%s'. Ignoring.\n", current_arg);
                }
            } else if (string_eq(current_arg, "help")) {
                print_help();
                return ParseResult_Stop;
            } else if (string_eq(current_arg, "version")) {
                fprintf(stdout, "%s\n", QUICK_SCRIPT_VERSION);
                return ParseResult_Stop;
            } else if (string_eq(current_arg, "template")) {
                current_arg = get_arg(args, ++arg_index, num_args);
                if (current_arg == NULL) {
                    fprintf(stdout, "--template should be followed by a template string.\n");
                    return ParseResult_Invalid;
                }

                if (options->template_string == NULL) {
                    options->template_string = string_new(current_arg);
                } else {
                    // Override the former template string
                    options->template_string = string_copy(options->template_string, current_arg);
                }
            } else {
                // If not a special argument type, try and interpret as a variable name value pair
                char * varname = current_arg;
                if (!is_identifier(varname)) {
                    fprintf(stdout, "Variable name '%s' is not a valid name. Variables must start with a letter, and consist only of letters, numbers and '-' and '_' (e.g. --some-variable_1, --NAME1).\n", varname);
                    return ParseResult_Invalid;
                }

                // Advance one argument to get the value
                char * varval = get_arg(args, ++arg_index, num_args);
                if (varval == NULL) {
                    fprintf(stdout, "Missing value for variable '%s'\n", varname);
                    return ParseResult_Invalid;
                }
                options->variables = template_set(options->variables, varname, varval);
            }
        } else if (is_identifier(current_arg)) {
            if (options->action_name != NULL) {
                fprintf(stdout, "Only one action name can ge given. Got %s and %s.\n", options->action_name, current_arg);
                return ParseResult_Invalid;
            }
            // No action assigned yet, make the argument the action name
            options->action_name = string_new(current_arg);
        } else {
            printf("Unknown argument: %s\n", current_arg);
            return ParseResult_Invalid;
        }
        // Advance to the next argument
        current_arg = get_arg(args, ++arg_index, num_args);
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
