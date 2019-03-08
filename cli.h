#pragma once

#include "base.h"
#include "templates.h"
#include "string.h"

enum ParseResult
{
    // Parsed OK, proceed with execution
    ParseResult_Procceed,
    // Parsed OK, stop execution
    ParseResult_Stop,
    // User gave invalid arguments (reason printed during parsing)
    ParseResult_Invalid,
    // An error happened (error printed during parsing)
    ParseResult_Error,
};

struct CommandLineOptions {
    /* The action to run (cannot be used together with template_string) */
    String action_name = 0;

    /* The template string to to use (cannot be used together with action_name) */
    String template_string = 0;

    /* A list of config files given at the command line. Multiple config files can be given
     * by specifying --config multiple times. The order matters, the first config file
     * to be check is the last one given. */
    StringList * config_files = 0;

    /* Setting this flag will print the command that would have run to stdout instead of
     * running it. */
    bool dry_run = false;

    /* Debug mode flag. Will print verbose debugging information. */
    bool verbose = false;

    /**
     * Flag that indicates that the user wants to print the generated help string for the
     * provided action name
     */
    bool print_action_help = false;

    /* Variables */
    VarList * variables;
};

ParseResult parse_cli_args(CommandLineOptions * options, int num_args, char ** args);

void init_options(CommandLineOptions * options);

/* Frees all resources claimed by the CommandLineOptions */
void free_cli_options_resources(CommandLineOptions options);
