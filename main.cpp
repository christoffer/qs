#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "base.h"
#include "cli.h"
#include "configs.h"
#include "files.h"
#include "help_text.h"

#define QUICK_SCRIPT_VERSION "1.1.0"

static void
print_version()
{
    fprintf(stdout, "%s\n", QUICK_SCRIPT_VERSION);
}

static void
print_usage(const char* exec_name)
{
    printf("Usage:\n");
    printf("%s [options] <action name> [--help] [<action arguments>, ...]\n", exec_name);
    printf("%s [options] --template <template string> [<action arguments>, ...]\n", exec_name);
    printf("  --help to see more help.\n");
    printf("  --actions to see a list of available actions.\n");
}

static void
print_available_actions(StringList* config_filepaths)
{
    StringList* config_path_item = config_filepaths;
    StringList* seen = 0;
    bool did_print_header = false;
    while (config_path_item) {
        String config_path = config_path_item->string;
        StringList* action_names = 0;
        if (config_get_action_names(config_path, &action_names)) {
            if (!did_print_header) {
                fprintf(stdout, "Available actions:\n");
                did_print_header = true;
            }
            StringList* action_name_item = action_names;
            while (action_name_item) {
                String action_name = action_name_item->string;
                bool shadowed = string_list_contains(seen, action_name);
                if (!shadowed) {
                    fprintf(stdout, " - %-35s (%s)\n", action_name, config_path);
                    seen = string_list_add_front_dup(seen, action_name);
                }
                action_name_item = action_name_item->next;
            }
            string_list_free(action_names);
        }
        config_path_item = config_path_item->next;
    }
    string_list_free(seen);
}

/**
 * Break a filepath at the last forward slash ('/').
 * If there are no '/' characters in the provided filepath, a '.' character is returned.
 * The trailing '/' is not included after calling this function.
 */
static void dirname(String filepath)
{
    u32 offset = string_len(filepath);
    while (offset && filepath[offset] != '/')
        offset--;
    if (!offset) {
        filepath[0] = '.';
        filepath[1] = '\0';
    } else {
        filepath[offset] = '\0';
    }
}

static void
exec_with_options(CommandLineOptions options, String shell_command, char* cwd)
{
    String cmd = string_new("cd ");
    cmd = string_append(cmd, cwd ? cwd : ".");
    cmd = string_append(cmd, "; QS_RUN_DIR=");

    char rundir[PATH_MAX] = { 0 };
    if (realpath(".", rundir)) {
        cmd = string_append(cmd, rundir);
    } else {
        fprintf(stderr, "Error: Failed to resolve current directory\n");
        string_free(cmd);
        return;
    }
    cmd = string_append(cmd, "; ");
    cmd = string_append(cmd, shell_command);

    if (options.dry_run) {
        fprintf(stdout, "Would run: %s\n", cmd);
    } else {
        if (options.verbose) {
            fprintf(stdout, "Running: %s\n", cmd);
        }
        system(cmd);
    }

    string_free(cmd);
}

static void
populate_options_with_default_config_files(CommandLineOptions* options)
{
    StringList* default_config_files = resolve_default_config_files();

    if (options->config_files) {
        // Add the default configs files after the user provided ones
        StringList* end = options->config_files;
        while (end && end->next)
            end = end->next;
        end->next = default_config_files;
    } else {
        // Set the config files to just the default ones
        options->config_files = default_config_files;
    }

    if (options->verbose && options->config_files) {
        fprintf(stdout, "Searching the following configuration files:\n");
        StringList* node = options->config_files;
        while (node) {
            fprintf(stdout, " - %s\n", node->string);
            node = node->next;
        }
    }
}

enum ErrorType {
    ErrorType_None = 0,
    ErrorType_Error = 1,
    ErrorType_User = 2,
};

static ErrorType
process_options(CommandLineOptions* options, const char* program_name)
{
    // Handle no argument invokation
    if (options->no_arguments_given) {
        print_usage(program_name);
        return ErrorType_User;
    }

    // Handle --help
    if (options->print_help) {
        print_help();
        return ErrorType_None;
    }

    // Handle --version
    if (options->print_version) {
        print_version();
        return ErrorType_None;
    }

    if (options->print_available_actions) {
        populate_options_with_default_config_files(options);
        print_available_actions(options->config_files);
        return ErrorType_None;
    }

    if (options->action_name && options->template_string) {
        fprintf(stdout, "Error: Must provide either an action name or a template string (--template), not both.\n");
        return ErrorType_User;
    }

    if (!options->action_name && !options->template_string) {
        fprintf(stdout, "Error: Must provide either an action name or a --template\n\n");
        print_usage(program_name);
        return ErrorType_User;
    }

    if (options->template_string) {
        if (options->verbose) {
            fprintf(stdout, "Resolved template: %s\n", options->template_string);
        }
        String command = template_render(options->template_string, options->variables);
        if (command) {
            exec_with_options(*options, command, 0);
            string_free(command);
            return ErrorType_None;
        }
        return ErrorType_User;
    }

    if (options->action_name) {
        // User gave an action name. Dig into the config files and try to resolve it.
        populate_options_with_default_config_files(options);

        // Search through the configuration files for an action with the given name in order to find
        // the template.
        char* action_name = options->action_name;
        char* config_path = 0;

        // Loop through the configuration files and look for the first declaration of the
        // sought action.
        ResolvedTemplateResult template_res;
        StringList* config_file = options->config_files;
        while (config_file) {
            config_path = config_file->string;
            template_res = resolve_template_for_action(config_path, action_name);
            if (template_res.parse_error) {
                return ErrorType_Error;
            } else if (template_res.template_string) {
                // Found the action template, stop looking
                break;
            }
            config_file = config_file->next;
        }

        if (template_res.template_string) {
            // Successfully resolved a valid template for the action
            if (options->verbose) {
                fprintf(stdout, "Resolved template: %s\nFrom: %s\n", template_res.template_string, config_path ? config_path : "--template");
                if (template_res.vars) {
                    fprintf(stdout, "with predefined variable values:\n");
                    VarList* vars = template_res.vars;
                    while (vars) {
                        fprintf(stdout, " - ${%s} => %s\n", vars->name, vars->value);
                        vars = vars->next;
                    }
                }
            }

            ErrorType error;
            if (options->print_action_help) {
                String usage = template_generate_usage(template_res.template_string, options->action_name);
                if (usage) {
                    fprintf(stdout, "%s", usage);
                    string_free(usage);
                    error = ErrorType_None;
                } else {
                    fprintf(stderr, "Invalid action template: %s\n", template_res.template_string);
                    error = ErrorType_Error;
                }
            } else {
                // Set the config path for the template render function to use when setting the
                // command QS_RUN_DIR varibable
                if (config_path)
                    dirname(config_path);

                // Merge the user defined variables into the config file provided variables
                VarList* merged_vars = template_merge(template_res.vars, options->variables);
                String command = template_render(template_res.template_string, merged_vars);
                template_free(merged_vars);
                if (command) {
                    exec_with_options(*options, command, config_path);
                    string_free(command);
                    error = ErrorType_None;
                } else {
                    fprintf(stderr, "Invalid action template: %s\n", template_res.template_string);
                    error = ErrorType_Error;
                }
            }

            // Free the template result resources
            string_free(template_res.template_string);
            template_free(template_res.vars);
            return error;
        } else {
            // Failed to find an template for the action
            fprintf(stdout, "Could not find action with name: %s\n", action_name);
            return ErrorType_User;
        }
    }

    assert(false); // All possible combinations should have been exhausted at this point
}

int main(int argc, char** argv)
{
    // parse command line arguments
    CommandLineOptions options = {};

    ParseResult parse_result = parse_cli_args(&options, argc, argv);
    switch (parse_result) {
    case ParseResult_Error:
        free_cli_options_resources(options);
        exit(ErrorType_Error);
    case ParseResult_Invalid:
        free_cli_options_resources(options);
        exit(ErrorType_User);
    case ParseResult_Ok:
        /* fallthrough */;
    }

    ErrorType error = process_options(&options, argv[0]);
    free_cli_options_resources(options);
    exit(error);
}
