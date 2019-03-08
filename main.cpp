#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>

#include "base.h"
#include "files.h"
#include "cli.h"
#include "configs.h"

/**
 * Break a filepath at the last forward slash ('/').
 * If there are no '/' characters in the provided filepath, a '.' character is returned.
 * The trailing '/' is not included after calling this function.
 */
static void dirname(String filepath) {
    u32 offset = string_len(filepath);
    while (offset && filepath[offset] != '/') offset--;
    if (!offset) {
        filepath[0] = '.';
        filepath[1] = '\0';
    } else {
        filepath[offset] = '\0';
    }
}

static void
exec_with_options(CommandLineOptions options, String shell_command, char * cwd)
{
    String cmd = string_new("cd ");
    cmd = string_append(cmd, cwd ? cwd : ".");
    cmd = string_append(cmd, "; QS_RUN_DIR=");

    char rundir[PATH_MAX] = {0};
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

enum ErrorType {
    ErrorType_None = 0,
    ErrorType_Error = 1,
    ErrorType_User = 2,
};

int
main(int argc, char **argv)
{
    // parse command line arguments
    CommandLineOptions options = {};

    ParseResult parse_result = parse_cli_args(&options, argc, argv);
    switch(parse_result)
    {
        case ParseResult_Stop:        exit(ErrorType_None);
        case ParseResult_Error:       exit(ErrorType_Error);
        case ParseResult_Invalid:     exit(ErrorType_User);
        case ParseResult_Procceed:    /* fallthrough */;
    }

    ErrorType error = ErrorType_None;

    String command = 0;
    assert(options.action_name || options.template_string);
    if (options.template_string) {
        if (options.verbose) {
            fprintf(stdout, "Resolved template: %s\n", options.template_string);
        }
        command = template_render(options.variables, options.template_string);
        if (command) {
            exec_with_options(options, command, 0);
        } else {
            error = ErrorType_User;
        }
    } else {
        // User gave an action name. Dig into the config files and try to resolve it.
        options.config_files = resolve_and_append_default_config_files(options.config_files);
        if (options.verbose && options.config_files)
        {
            fprintf(stdout, "Searching the following configuration files:\n");
            StringList * node = options.config_files;
            while (node) {
                fprintf(stdout, " - %s\n", node->string);
                node = node->next;
            }
        }

        // Search through the configuration files for an action with the given name in order to find
        // the template.
        String resolved_template = string_new();
        VarList * config_defined_variables = 0;
        char * action_name = options.action_name;
        ResolveTemplateRes resolve_template_res = ResolveTemplateRes_Missing;
        char * config_path = 0;
        StringList * node = options.config_files;
        while (node) {
            config_path = node->string;
            resolve_template_res = resolve_template_for_action(config_path, action_name, &resolved_template, &config_defined_variables);
            if (resolve_template_res != ResolveTemplateRes_Missing)
            {
                break;
            }
            node = node->next;
            template_free(config_defined_variables);
        }

        if (resolve_template_res == ResolveTemplateRes_Found) {
            if (options.verbose) {
                fprintf(stdout, "Resolved template: %s\nFrom: %s\n", resolved_template, config_path ? config_path : "--template");
                if (config_defined_variables) {
                    fprintf(stdout, "with predefined variable values:\n");
                    VarList * vars = config_defined_variables;
                    while(vars) {
                        fprintf(stdout, " - ${%s} => %s\n", vars->name, vars->value);
                        vars = vars->next;
                    }
                }
            }

            if (options.print_action_help) {
                String usage = template_get_usage(resolved_template, options.action_name);
                if (usage) {
                    fprintf(stdout, "%s", usage);
                }
            } else {
                // Merge the user defined variables into the config file provided variables
                VarList * render_vars = 0;
                VarList * varptr = config_defined_variables;
                while (varptr) {
                    render_vars = template_set(render_vars, varptr->name, varptr->value);
                    varptr = varptr->next;
                }
                varptr = options.variables;
                while (varptr) {
                    render_vars = template_set(render_vars, varptr->name, varptr->value);
                    varptr = varptr->next;
                }

                // Set the config path for the template render function to use when setting the
                // command QS_RUN_DIR varibable
                if (config_path) dirname(config_path);
                command = template_render(render_vars, resolved_template);

                if (command) {
                    exec_with_options(options, command, config_path);
                } else {
                    error = ErrorType_User;
                }
            }
        } else if (resolve_template_res == ResolveTemplateRes_Missing) {
            fprintf(stdout, "Could not find action with name: %s\n", action_name);
            error = ErrorType_User;
        } else if (resolve_template_res == ResolveTemplateRes_Error) {
            error = ErrorType_Error;
        }

        string_free(resolved_template);
    }

    free_cli_options_resources(options);
    exit(error);
}
