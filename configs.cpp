#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>

#include "base.h"
#include "configs.h"
#include "string.h"
#include "files.h"

static String
find_source_root_dir(const char * start_path)
{
    String curpath = string_new(start_path);
    u32 curpath_len = string_len(curpath);

    // Only look for .git directories. Other source roots TBD
    char wanted_entry[] = "/.git";

    bool found_source_root = false;
    String candidate = string_new();
    while (!found_source_root)
    {
        candidate = string_copy(candidate, curpath);
        candidate = string_append(candidate, wanted_entry);
        if (is_readable_dir(candidate))
        {
            found_source_root = true;
            break;
        }

        if (curpath_len == 0)
        {
            break;
        }

        // move curpath up one level and check again
        while(curpath_len--)
        {
            if (curpath[curpath_len] == '/')
            {
                curpath[curpath_len] = '\0';
                break;
            }
        }

        if (curpath_len == 0)
        {
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
static StringList *
push_back_dup(StringList ** head, StringList * end, const char * content) {
    StringList * node = (StringList *) calloc(1, sizeof(StringList));
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

StringList *
resolve_and_append_default_config_files(StringList * config_files)
{
    /**
     * NOTE(christoffer) The order in which we resolve these is significant. The resulting list will
     * processed from start to end, and the first action match is the one that's picked.
     *
     * It's assumed here that whatever paths are already in the list are higher priority than any
     * of the config files resolved here, so we want to start appending to the end of the existing
     * list.
     */

    StringList * head = config_files;
    StringList * end = head;
    while(end && end->next) end = end->next;

    /* Resolve cwd config */
    {
        char local_config_path[PATH_MAX];
        if(realpath("./.qs.cfg", local_config_path) && is_readable_regfile(local_config_path)) {
            end = push_back_dup(&head, end, local_config_path);
        }
    }

    /* Resolve source root config */
    {
        char cwd_path[PATH_MAX];
        String source_root;
        if (realpath(".", cwd_path) && (source_root = find_source_root_dir(cwd_path)))
        {
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
        char * xdg_config_home_env = getenv("XDG_CONFIG_HOME");

        String xdg_config_home_dir = 0;
        if (xdg_config_home_env) {
            // User has set a non-default directory. Use that as the base.
            xdg_config_home_dir = string_new(xdg_config_home_env);
        } else {
            // Fall back to the default config home directory of $HOME/.config
            char * home = getenv("HOME");
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
                    && is_readable_regfile(resolved_default_config_path)
            ) {
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
    while((offset < content_len) && (content[offset] == ' '))
    {
        offset++;
    }
    return offset;
}

static u32
read_identifier(u32 start, String content, String * value)
{
    u32 offset = start;
    while(offset < string_len(content) && is_identifier_char(content[offset]))
    {
        offset++;
    }
    if (offset > start)
    {
        *value = string_copy(*value, (content + start), offset - start);
    }
    return offset;
}

static u32
read_until_newline(u32 start, String content, String * value)
{
    u32 offset = start;
    while(offset < string_len(content) && content[offset] != '\n') {
        offset++;
    }
    if (value && (offset > start)) {
        *value = string_copy(*value, (content + start), offset - start);
    }
    return offset;
}

static ResolveTemplateRes
resolve_template(const char * action_name, const char * filepath, String filecontent, String * resolved_template, VarList ** config_vars)
{
    // valid lines are:
    // - NEWLINE
    // - [WS] VARIABLE [WS] = [WS] VARIABLE VALUE
    // - [WS] ACTION [WS] = [WS] TEMPLATE NEWLINE
    // - [WS] COMMENT NEWLINE
    u32 offset = 0;
    u32 new_offset = 0;

    String value = string_new();
    String pending_var_name = 0;
    bool action_matched_this_line = true;
    bool did_dupe_warning = false;

    ResolveTemplateRes result = ResolveTemplateRes_Missing;

    /**
     * NOTE(christoffer) We could be efficient here and return the first match. But we
     * chose to parse the entire config file in order to give consistent errors when it's used.
     */
    u32 content_len = string_len(filecontent);
    while (offset < content_len)
    {
        offset = skip_whitespace(offset, filecontent);
        if ((new_offset = read_identifier(offset, filecontent, &value)) > offset)
        {
            offset = new_offset;
            offset = skip_whitespace(offset, filecontent);

            // Found and parsed an identifier. We expect it to be followed by either
            // - a ':=' (if it's a variable definition)
            // - a '=' (if it's an action definition)

            if (
                    (offset + 2 < content_len)
                    && filecontent[offset] == ':'
                    && filecontent[offset + 1] == '=') {
                // Variable definition
                offset += 2; // eat :=
                pending_var_name = string_new(value);
            } else if (filecontent[offset] == '=') {
                // Action definition
                offset += 1; // eat =
                action_matched_this_line = string_eq(action_name, value);
                if (action_matched_this_line && (result == ResolveTemplateRes_Found) && !did_dupe_warning)
                {
                    fprintf(stdout, "Warning: duplicate action definition: %s (first one is used)\n", action_name);
                    action_matched_this_line = false;
                    did_dupe_warning = true;
                }

                if (action_matched_this_line && result != ResolveTemplateRes_Error) {
                    result = ResolveTemplateRes_Found;
                }
            } else {
                // Error
                fprintf(stderr, "Error in %s: Expected '=' or ':='\n", filepath);
                result = ResolveTemplateRes_Error;
                break;
            }

            // Eat whitespace after the (:)= and then parse the rest of the line as the value
            offset = skip_whitespace(offset, filecontent);
            if (filecontent[offset] == '#')
            {
                fprintf(stderr, "Error in %s: Value cannot start with '#'\n", filepath);
                result = ResolveTemplateRes_Error;
                break;
            }
            if ((new_offset = read_until_newline(offset, filecontent, &value)) > offset)
            {
                if (pending_var_name) {
                    // Parsed a variable name at the start of the line, set the value
                    *config_vars = template_set(*config_vars, pending_var_name, value);
                } else if (action_matched_this_line) {
                    // Found a template for the right action, set the return value but continue
                    // to parse the file.
                    *resolved_template = string_copy(*resolved_template, value);
                }
                offset = new_offset;
            } else {
                // Error -- didn't get more content
                fprintf(stderr, "Error in %s: No value after '='\n", filepath);
                result = ResolveTemplateRes_Error;
                break;
            }
            assert((filecontent[offset] == '\n') || (filecontent[offset] == '\0'));
        }
        else if (filecontent[offset] == '#')
        {
            offset++; // eat '#'
            offset = read_until_newline(offset, filecontent, 0);
        }
        else if (filecontent[offset] == '\n')
        {
            offset++;
        }
        else
        {
            fprintf(stderr, "Error: Unexpected character: '%c'\n", filecontent[offset]);
            result = ResolveTemplateRes_Error;
            break;
        }

        if (pending_var_name) {
            string_free(pending_var_name);
            pending_var_name = 0;
        }
    }

    string_free(value);
    return result;
}

struct ActionTemplatePair {
    String action_name;
    String template_string;
};

struct ActionTemplatePairs {
    ActionTemplatePair * pairs;
    u32 count;
    u32 alloc_count;
};

ResolveTemplateRes
resolve_template_for_action(char * config_file_path, char * action_name, String * resolved_template, VarList ** config_vars) {
    String filecontent;

    if (!(filecontent = read_entire_file(config_file_path))) {
        fprintf(stderr, "Error: failed to read config file: %s, aborting.\n", config_file_path);
        return ResolveTemplateRes_Error;
    }

    ResolveTemplateRes result = resolve_template(action_name, config_file_path, filecontent, resolved_template, config_vars);
    string_free(filecontent);
    return result;
}
