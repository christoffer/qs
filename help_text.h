#include <stdio.h>

void print_help();

void print_help()
{
    fprintf(stdout, R"help(

qs (quick-scripts): A tiny utility for keeping a catalogue of one-liners.

Usage:
qs [options] <action name> [--help] [<action arguments>, ...]
qs [options] --template <template string> [<action arguments>, ...]

Options:
  --actions:  List all available actions and exit.
  --dry-run:  Print the command that would have run, don't actually run it.
  --verbose:  Print more information while executing.
  --template: Ignore the preconfigured templates and use an explicit template instead.
              Cannot be used together with <action name>
  --version:  Print the current version and exit.
  --help:     Show this help and exit.
              Providing --help after an action name will print the help for that action and exit.
  --config:   Include additional configuration file when looking for the action (can be used more than once).

Actions:
  Actions are named oneliner scripts (templated, see below) that are executed using bash in the directory
  where they where defined. This allows for consistent relative paths to other executables or files.

  The environment variable $QS_RUN_DIR is set to the current working directory where the qs program
  was run.

  They are configured in one of the following config files (in order).
    - `.qs.cfg` file in the current directory
    - `.qs.cfg` file in a parent git source-root of the current directory
    - `$XDG_CONFIG_HOMEqs/default.cfg`
    - `$HOME.configs/qs/default.cfg` (unless XDG_CONFIG_HOME is set)

  Additional configuration files can be provided using --config, and will have a higher priority
  than the default ones in case the same action name ocurrs multiple times.

Action arguments:
  Depending on the action template, actions can support both positional arguments and named arguments.

  Named arguments are provided using --<argument name> <argument value>.
  Any --argument after the action name, except for --help, will be interpreted as a named
  argument and provided to the template for expansion using ${name}.

  All other arguments will provided to the template in the order they appear (e.g. the first
  argument is ${0}, the second ${1}, etc.)

  If --help is provided anywhere after the action name, the help string for the action will
  be printed, and the program will exit.

Configration files:
  The format for configuration files is:
  <action name> = <template string>
  <default argument> := <default argument value>

  Comments are allowed using '#' at the start of the line.
  Whitespace and emtpy lines are ignored.

  Template strings or argument values is anything (except the leading whitespace) following
  the = or := to the end of the line.

  Valid action-, or argument names follow the format [a-z][a-zA-Z0-9_-]+
  (e.g.  'fooBar', 'thing1', 'my_arg', 'my-arg-1').

Templates:
  Templates can expand positional arguments using ${0}, ${1}, (etc) placeholders.
  Named arguments can be expanded using ${foobar}

  For example, calling 'qs foo --baz qux' will use the following substituions in the template:
    ${0} => 'foo'
    ${baz} => 'qux'

  Templates allow for conditional sections using `${arg?}`, `${else}` (optional), and `${end}`.

  For example:
    qs --name 'Christoffer' --template '${name?}echo "Hi ${name}!"${else}echo "Anyone there?"${end}'
    #=> 'Hi Christoffer!'
    qs --template '${name?}echo "Hi ${name}!"${else}echo "Anyone there?"${end}'
    #=> 'Anyone there?'

  Nested conditional blocks are allowed.

  To use a literal `$` in the template, escape it using another `$` (e.g. `$$PATH`)
  Anything that's not an argument substition is passed as-is to bash (there's no escaping).

Examples:

  .qs.cfg:
  # -----------------------------------
  # Opens Google in a browser
  engine := google
  search = xdg-open https://www.${engine}.com/${0?}?q=${0}${end}
  # -----------------------------------

 $> qs search                     # opens https://www.google.com
 $> qs search cats                # opens https://www.google.com?q=cats
 $> qs search cats --engine duck  # opens https://www.duck.com?q=cats
)help");
}
