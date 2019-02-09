#pragma once

#include <limits.h>

#include "base.h"
#include "string.h"
#include "templates.h"

enum TokenType {
    TokenType_Undefined = 0,
    TokenType_Whitespace,
    TokenType_NewLine,
    TokenType_Comment,
    TokenType_ActionName,
    TokenType_Equal,
    TokenType_Template,
    TokenType_Invalid,
};

struct Token {
    TokenType type = TokenType_Undefined;
    u32 start = 0;
    u32 end = 0;
};

enum ResolveTemplateRes
{
    ResolveTemplateRes_Found,
    ResolveTemplateRes_Missing,
    ResolveTemplateRes_Error,
};

/* Loop through a list of default configuration file locations, and add each existing one to
 * the privided string list. Only adds existing files that can be read.
 *
 * The paths are searched in order of priority. This means that the configration file to search
 * first is added fist to the list. It's assumed that the list already contains configuration files
 * with higher priority than any one added by this function. */
StringList * resolve_and_append_default_config_files(StringList * config_files);

/**
 * Resolves a template string for given action by parsing a list of config
 * files. The order of the config files is significant, as the first matched
 * action is the one resolved.
 *
 * Returns a ResolveTemplateRes enum value which indicates the resolution (found, not found or error).
 * The resolved template string is written to 'resolved_template' only if the return value is found.
 */
ResolveTemplateRes resolve_template_for_action(char * config_file_path, char * action_name, String * resolved_template, VarList ** config_vars);
