#pragma once

#include "base.h"
#include "string.h"
#include "templates.h"

enum ParseResult {
    // Parsed OK, proceed with execution
    ParseResult_Ok,
    // User gave invalid arguments (reason printed during parsing)
    ParseResult_Invalid,
    // An error happened (error printed during parsing)
    ParseResult_Error,
};

/** The resulting configuration flags from parsing the CLI arguments given by the user. */
struct CommandLineOptions {
    // The action name
    String action_name = 0;

    // Ad-hoc action template string
    String action_template = 0;

    // List of configuration files given as arguments to the program.
    // Multiple ones can be given. The list is ordered by configuration priority
    // preference (most preferred is first in the list, least preferred is last).
    StringList* config_files = 0;

    // Flags

    // Prevent actually running the rendered template, just print it.
    bool dry_run = false;

    // Print verbose information during execution.
    bool verbose = false;

    // Print the version.
    bool print_version = false;

    // Print the help string and exit
    bool print_help = false;

    // Print help for the given action_name
    bool print_action_help = false;

    // List all available actions
    bool print_available_actions = false;

    // No arguments passed
    bool no_arguments_given = false;

    // List of variables passed on the command line.
    // Positional arguments will be named "0", "1", etc. Named arguments will
    // have the given name (minus the leading --).
    VarList* variables;
};

ParseResult parse_cli_args(CommandLineOptions* options, int num_args, char** args);

void init_options(CommandLineOptions* options);

/* Frees all resources claimed by the CommandLineOptions */
void free_cli_options_resources(CommandLineOptions options);
