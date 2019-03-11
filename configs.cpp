#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "configs.h"
#include "files.h"

struct ActionTemplatePair {
    String action_name = 0;
    String template_string = 0;
    ActionTemplatePair* next = 0;
};

static void
_free_pairs(ActionTemplatePair* head)
{
    ActionTemplatePair* pair = head;
    while (pair) {
        string_free(pair->action_name);
        string_free(pair->template_string);
        ActionTemplatePair* dead = pair;
        pair = pair->next;
        free(dead);
    }
}

static String
find_source_root_dir(const char* start_path)
{
    String curpath = string_new(start_path);
    u32 curpath_len = string_len(curpath);

    // Only look for .git directories. Other source roots TBD
    char wanted_entry[] = "/.git";

    bool found_source_root = false;
    String candidate = string_new();
    while (!found_source_root) {
        candidate = string_copy(candidate, curpath);
        candidate = string_append(candidate, wanted_entry);
        if (is_readable_dir(candidate)) {
            found_source_root = true;
            break;
        }

        if (curpath_len == 0) {
            break;
        }

        // move curpath up one level and check again
        while (curpath_len--) {
            if (curpath[curpath_len] == '/') {
                curpath[curpath_len] = '\0';
                break;
            }
        }

        if (curpath_len == 0) {
            break;
        }
    }
    string_free(candidate);

    if (found_source_root) {
        return curpath;
    }

    string_free(curpath);
    return 0;
}

/**
 * Allocates memory for a new node and it's content. Appends to the given end node (if set)
 * and returns the the new end (the new node)
 * */
static StringList*
push_back_dup(StringList** head, StringList* end, const char* content)
{
    StringList* node = (StringList*)calloc(1, sizeof(StringList));
    node->string = string_new(content);
    node->next = 0;

    if (end) {
        end->next = node;
    }

    if (*head == 0) {
        *head = node;
    }

    return node;
}

StringList*
resolve_default_config_files()
{
    /**
     * NOTE(christoffer) The order in which we resolve these is significant. The resulting list will
     * processed from start to end, and the first action match is the one that's picked.
     */
    StringList* head = 0;
    StringList* end = head;

    /* Resolve cwd config */
    {
        char local_config_path[PATH_MAX];
        if (realpath("./.qs.cfg", local_config_path) && is_readable_regfile(local_config_path)) {
            end = push_back_dup(&head, end, local_config_path);
        }
    }

    /* Resolve source root config */
    {
        char cwd_path[PATH_MAX];
        String source_root;
        if (realpath(".", cwd_path) && (source_root = find_source_root_dir(cwd_path))) {
            // NOTE(christoffer) If the cwd is the source root, then we'll add the same file twice.
            // While it has no functional difference, we'd like to avoid the unnecessery work, so
            // we skip the source root in this case and rely on the cwd config being picked up in
            // a subsequent step.
            if (!string_eq(cwd_path, source_root)) {
                source_root = string_append(source_root, "/.qs.cfg");
                if (is_readable_regfile(source_root)) {
                    end = push_back_dup(&head, end, source_root);
                }
            }
            string_free(source_root);
        }
    }

    /* Resolve config config file in XDG_CONFIG_HOME */
    {
        // Resolve the XDG_CONFIG_HOME. This is either the environment variable set
        // by the user, or a defined default as per:
        // https://wiki.archlinux.org/index.php/XDG_Base_Directory
        char* xdg_config_home_env = getenv("XDG_CONFIG_HOME");

        String xdg_config_home_dir = 0;
        if (xdg_config_home_env) {
            // User has set a non-default directory. Use that as the base.
            xdg_config_home_dir = string_new(xdg_config_home_env);
        } else {
            // Fall back to the default config home directory of $HOME/.config
            char* home = getenv("HOME");
            if (home) {
                xdg_config_home_dir = string_new(home);
                xdg_config_home_dir = string_append(xdg_config_home_dir, "/.config");
            }
        }

        if (xdg_config_home_dir) {
            String default_config_path = string_new(xdg_config_home_dir);
            default_config_path = string_append(default_config_path, "/qs/default.cfg");
            string_free(xdg_config_home_dir);

            char resolved_default_config_path[PATH_MAX];
            if (
                realpath(default_config_path, resolved_default_config_path)
                && is_readable_regfile(resolved_default_config_path)) {
                end = push_back_dup(&head, end, resolved_default_config_path);
            }
            string_free(default_config_path);
        }
    }

    return head;
}

static u32
skip_whitespace(u32 start, String content)
{
    u32 offset = start;
    u32 content_len = string_len(content);
    while ((offset < content_len) && (content[offset] == ' ')) {
        offset++;
    }
    return offset;
}

static u32
read_identifier(u32 start, String content, String* value)
{
    u32 offset = start;
    u32 content_len = string_len(content);
    while ((offset < content_len) && is_identifier_char(content[offset])) {
        offset++;
    }
    if (offset > start) {
        *value = string_copy(*value, (content + start), offset - start);
    }
    return offset;
}

static u32
read_until_newline(u32 start, String content, String* value)
{
    u32 offset = start;
    while (offset < string_len(content) && content[offset] != '\n') {
        offset++;
    }
    if (value && (offset > start)) {
        *value = string_copy(*value, (content + start), offset - start);
    }
    return offset;
}

static ActionTemplatePair*
remove_duplicate_actions(ActionTemplatePair* pairs, const char* filepath)
{
    ActionTemplatePair *head = pairs, *node = head, *prev = 0;
    StringList* seen_actions = 0;
    while (node) {
        if (string_list_contains(seen_actions, node->action_name)) {
            fprintf(stdout, "Warning: duplicate action name: %s (in %s)\n", node->action_name, filepath);

            // prev should always have been set, can't detect dupes withouth checking
            // at least two
            assert(prev);

            // Take the current 'node' out of the list and kill it
            prev->next = node->next;
            ActionTemplatePair* dead = node;
            node = node->next;
            string_free(dead->action_name);
            string_free(dead->template_string);
            free(dead);
        } else {
            seen_actions = string_list_add_front_dup(seen_actions, node->action_name);
            prev = node;
            node = node->next;
        }
    }
    string_list_free(seen_actions);
    return head;
}

static void
print_error(const char* message, const char* filepath)
{
    fprintf(stderr, "Error in %s: %s\n", filepath, message);
}

static bool
parse_config(const char* filepath, ActionTemplatePair** result_pairs, VarList** result_vars)
{
    String filecontent;
    if (!(filecontent = read_entire_file(filepath))) {
        print_error("Failed to read config file. Aborting", filepath);
        string_free(filecontent);
        return false;
    }
    u32 content_len = string_len(filecontent);

    // The head, and the tail of the pair list
    ActionTemplatePair* head = 0;
    ActionTemplatePair* end = 0;

    // The list of variables declared in the config file
    VarList* vars = 0;

    // Error flag set if the config file is invalid
    bool error = false;

    // Temporary storage variable for various values
    String value = string_new();

    // The parsed variable name to read a value for
    String pending_var_name = string_new();

    // The parse action name to read a template for
    String pending_action_name = string_new();

    // Offset into filecontent buffer we're currently reading
    u32 offset = 0;

    // Speculative offset after attempting to parse a certain sequence of bytes
    // (e.g. an identifier). We keep this separate to be able to compare them
    // in order to detect if the sequence was read or not.
    u32 new_offset = 0;

    // Parse the config linewise
    while (offset < content_len) {
        string_clear(pending_var_name);
        string_clear(pending_action_name);

        // Chew up any leading whitespace of the line
        offset = skip_whitespace(offset, filecontent);

        // Inspect the first non-whitespace content of the line
        if (!filecontent[offset]) {
            // EOF
            break;
        } else if (filecontent[offset] == '#') {
            // Rest of the line is a comment
            offset = read_until_newline(offset, filecontent, 0);
        } else if (filecontent[offset] == '\n') {
            // Empty-, or whitespace only line
            offset++;
        } else if ((new_offset = read_identifier(offset, filecontent, &value)) > offset) {
            // Found and parsed an identifier. We expect it to be followed by either
            // - a ':=' (if it's a variable definition)
            // - a '=' (if it's an action definition)
            offset = new_offset;
            offset = skip_whitespace(offset, filecontent);

            if (
                ((offset + 1) < content_len)
                && filecontent[offset] == ':'
                && filecontent[offset + 1] == '=') {
                // Variable (:=) declaration
                offset += 2; // eat :=
                pending_var_name = string_copy(pending_var_name, value);
            } else if (filecontent[offset] == '=') {
                // Action (=) declaration
                offset += 1; // eat =
                pending_action_name = string_copy(pending_action_name, value);
            } else {
                print_error("Expected '=' or ':='", filepath);
                error = true;
                offset = read_until_newline(offset, filecontent, 0);
                continue;
            }

            // Eat whitespace after the =/:= and then parse the rest of the line as the value
            offset = skip_whitespace(offset, filecontent);

            // Special case. We don't allow the value to start with a comment because it's
            // a bit ambiguous: "action = # is this a value or comment?"
            if (filecontent[offset] == '#') {
                if (string_len(pending_action_name)) {
                    print_error("Action template cannot start with '#'", filepath);
                } else if (string_len(pending_var_name)) {
                    print_error("Argument value cannot start with '#'", filepath);
                } else {
                    assert(false);
                }
                error = true;
                offset = read_until_newline(offset, filecontent, 0);
                continue;
            }

            if ((new_offset = read_until_newline(offset, filecontent, &value)) > offset) {
                // Got value, decide what the assign it to based on which pending variable
                // that was set.
                if (string_len(pending_var_name)) {
                    // Parsed a variable name at the start of the line, set the value
                    vars = template_set(vars, pending_var_name, value);
                } else if (string_len(pending_action_name)) {
                    ActionTemplatePair* node = (ActionTemplatePair*)calloc(1, sizeof(ActionTemplatePair));
                    assert(node);
                    node->action_name = string_new(pending_action_name);
                    node->template_string = string_new(value);
                    head = head ? head : node;
                    if (end)
                        end->next = node;
                    end = node;
                } else {
                    // Should always have gotten a var, action, or an error (that continues)
                    assert(false);
                }

                offset = new_offset;
            } else {
                if (string_len(pending_action_name)) {
                    print_error("No value after '='", filepath);
                } else if (string_len(pending_var_name)) {
                    print_error("No value after ':='", filepath);
                } else {
                    assert(false);
                }
                error = true;
                offset = read_until_newline(offset, filecontent, 0);
                continue;
            }

            // There shouldn't be a case where we didn't deplete the line or content
            assert((filecontent[offset] == '\n') || (filecontent[offset] == '\0'));
        } else {
            char errormsg[50] = { 0 };
            snprintf(errormsg, 50, "Unexpected character '%c' (%d)", filecontent[offset], filecontent[offset]);
            print_error(errormsg, filepath);
            error = true;
            offset = read_until_newline(offset, filecontent, 0);
            continue;
        }
    }

    // Free temporary data used during parsing
    string_free(filecontent);
    string_free(value);
    string_free(pending_action_name);
    string_free(pending_var_name);

    if (error) {
        _free_pairs(head);
        template_free(vars);
        return false;
    } else {
        head = remove_duplicate_actions(head, filepath);
        *result_pairs = head;
        *result_vars = vars;
        return true;
    }
}

bool config_get_action_names(char* config_file_path, StringList** action_names)
{
    StringList* found_action_names = 0;
    ActionTemplatePair* pairs = 0;
    VarList* vars = 0;

    if (parse_config(config_file_path, &pairs, &vars)) {
        ActionTemplatePair* pair = pairs;
        while (pair) {
            found_action_names = string_list_add_front_dup(found_action_names, pair->action_name);
            pair = pair->next;
        }
        _free_pairs(pairs);
        template_free(vars);
        *action_names = found_action_names;
        return true;
    }

    return false;
}

ResolvedTemplateResult
resolve_template_for_action(char* config_file_path, char* action_name)
{
    ActionTemplatePair* pairs = 0;
    VarList* vars = 0;

    ResolvedTemplateResult result = {};

    if (parse_config(config_file_path, &pairs, &vars)) {
        ActionTemplatePair* pair = pairs;
        bool found = false;
        while (pair) {
            if (string_eq(pair->action_name, action_name)) {
                result.template_string = string_new(pair->template_string);
                result.vars = template_merge(0, vars);
                found = true;
                break;
            }
            pair = pair->next;
        }
        _free_pairs(pairs);
        template_free(vars);
    } else {
        result.parse_error = true;
    }

    return result;
}
