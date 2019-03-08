qs (Quick Scripts): A tiny command line utility for executing reusable actions from editors.

Basically just a slightly more convenient way to run shell one-liners with the occational conditional or two.
Has some useful things like project-wise configurations etc too.

## Usage

```
qs [options] <action> [--var value, --var2 value2, ...]
```

Examples:

```
qs google-search --query "Christoffer Klang"
qs --config ~/configs/git-actions.config rebase-clean-master --repo ~/source-code/repos/plop
qs prettier --on my/file.js
qs --name "Christoffer" --template 'echo "Hello $name"'
```

## Options

`--dry-run`: Don't actually run the command, just print the command that would have been executed.
`--verbose`: Include verbose information when running the command.
`--version`: Print the version and exit.
`--template`: Use a provided template for the command (cannot be used together with an action).
`--config`: Include additional configuration files. Multiple ones can be given, the last given is the one scanned first.

Any other argument starting with `--` will be interpreted as a variable name, and the value will be used when expanding the template resolved by the action (or given as `--template`).

# Configuration

Actions are configured using key-value mapping of from an action name to a script template.

The configuration file format for an action is: `action-name=<action-template>`. 

Comments using `#` are also allowed. Empty lines are ignored.

Templates might contain variables using the syntax `${variable-name-here}`. Parts of the template can be
rendered conditionally using `${ variable? } <conditional part> ${end}`. The templates also support alternate
rendering paths using `${else}`: `${var?}<value if var is set>${else}<value if var is not set>${end}`. A
variable is considered not set if it's omitted or the value is an empty string `''`.

Conditionals can be nested.

## Variables

Template variables are given as double-dash arguments to qs (`--variable-name variable-value`). Default values
for template variables can be provided in the configuration files using `variable-name := <default value>`.

You can use `${0}`, `${1}`, etc for anonymous variables (0 is the first one, 1 is the second one, etc).

## Examples

```
# Opens Google in a browser
google-search = xdg-open https://www.google.com/${query?}?q=${query}${end}

# Creates a backup
source-dir := /home/christoffer/documents
backup=rsync ${source-dir} /Users/christoffer/backups/`date +%m-%d-%Y`.tar

# Tar the src directory into a named tarball
tar = tar -cf ${0} ./src
```

## Files

qs resolves an action by looking for it in a number of configuration files, going from the most local to the
least local. The first configuration file that has a definition for the action is used.

The configuration files that will be searched (in order):
- Explicit config files provided using `--config` (searched last to first)
- A `.qs.cfg` file in the current working directory.
- A `.qs.cfg` file in the parent git root of the working directory.
- `~/.config/qs/default.cfg` (see also `XDG_CONFIG_HOME`)

## Path references in config files

Once the template has been expanded, it is evaulated in the directory of the configuration file that defined
the action (or the current working directory if using `--template`). This means that paths used in actions can
be relative to the configuration file.

Actions can reference the current working directory where qa was run by using the environment variable `$QS_RUN_DIR`.
