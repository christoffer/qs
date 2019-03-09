#pragma once

#include "base.h"
#include "templates.h"
#include "string.h"

enum ParseResult
{
    // Parsed OK, proceed with execution
    ParseResult_Ok,
    // User gave invalid arguments (reason printed during parsing)
    ParseResult_Invalid,
    // An error happened (error printed during parsing)
    ParseResult_Error,
};

/** The resulting configuration flags from parsing the CLI arguments given by the user. */
struct CommandLineOptions {
    /** The passed action name */
    String action_name = 0;

    /** The passed template string */
    String template_string = 0;

    /**
     * A list of config files given at the command line.
     *
     * Multiple config files can be given by specifying --config more than once.
     * The order is significant; the first String in this list is the last one
     * given by the user.
     */
    StringList * config_files = 0;

    /** Flag for preventing actually running the rendered template, just print it. */
    bool dry_run = false;

    /** Flag for printing verbose information during execution. */
    bool verbose = false;

    /** Flag for printing the version. */
    bool print_version = false;

    /** Flag indicating that the user wishes to print the help string and exit. */
    bool print_help = false;

    /** Flag for printing help for the given action_name */
    bool print_action_help = false;

    /** Flag for listing all available actions */
    bool print_available_actions = false;

    /** Flag for when no any arguments were passed. */
    bool no_arguments_given = false;

    /** List of passed variables */
    VarList * variables;
};

ParseResult parse_cli_args(CommandLineOptions * options, int num_args, char ** args);

void init_options(CommandLineOptions * options);

/* Frees all resources claimed by the CommandLineOptions */
void free_cli_options_resources(CommandLineOptions options);
