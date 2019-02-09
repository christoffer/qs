#pragma once

#include "string.h"

struct VarList {
    String name = 0;
    String value = 0;
    VarList * next = 0;
};

/**
 * Set the variable with 'name' to 'value'. If a variable with 'name' already
 * exists, it's overwritten. Otherwise the new variable is appended at the end.
 */
VarList * template_set(VarList * vars, const char * name, const char * value);

/** Goes through the list of variables and calls string_free() on each name and value. */
void template_free(VarList * vars);

/**
 * Search through the template variables, stopping once a variable with 'name' has
 * been found and returns the corresponding value.
 * Returns 0 if no variable with 'name' was found.
 */
String template_get(VarList * vars, const char * name);

/**
 * Returns the template with variables substituted using values from the variable set.
 */
String template_render(VarList * vars, String template_string);
