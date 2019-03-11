#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

#include "templates.h"

enum TokenType {
    TT_None = 0,
    TT_Str,
    TT_Var,
    TT_If,
    TT_Else,
    TT_End,
};

struct LinkedToken {
    TokenType type = TT_None;
    String value;
    u32 start;
    u32 end;
    LinkedToken * next;
};

inline static void
linked_token_free(LinkedToken * token) {
    while(token) {
        LinkedToken * dead = token;
        token = token->next;
        if (dead->value) {
            string_free(dead->value);
        }
        free(dead);
    }
}

static void
print_error(const char * message, u32 start, u32 end, String template_string) {
    // Print a message like:
    //
    // Error: error message here
    // some template error somewhere
    //               ^^^^^
    //
    fprintf(stdout, "Error: %s.\n%s\n", message, template_string);
    u32 i = 0;
    while (i++ < start) printf(" ");
    while (i++ <= end) printf("^");
}

static void
add_token_and_reset(TokenType type, String value, u32 offset, LinkedToken ** head, LinkedToken ** end) {
    LinkedToken * new_token = (LinkedToken *) calloc(1, sizeof(LinkedToken));
    new_token->type = type;

    // We expect this function to be called once the entire token has been scanned,
    // and offset pointing to the first character of the next token.
    new_token->start = offset - string_len(value);
    new_token->end = offset;
    new_token->value = string_new(value);

    // Set the first token in the list if it's not already set
    if (!*head) *head = new_token;

    // If we have an end token set, link it with the new current before making
    // the new token the new end.
    if (*end) (*end)->next = new_token;
    *end = new_token;

    string_clear(value);
}

static LinkedToken *
tokenize_template(String template_string) {
    enum TokenizerMode {
        Literal,
        VarBlock,
    };

    // Flag set whenever any error has ocurred during parsing
    bool error = false;
    // Flag set whenever a variable has been seen inside a var block
    bool seen_variable = false;
    // Flag set when a $ character has been seen, expecting the next character to be either $ or {
    bool escape_mode = false;
    // Flag set when the next pass through the loop should consume any existing whitespace
    bool skip_next_whitespace = false;

    TokenizerMode mode = Literal;
    String curlit = string_new();

    u32 offset = 0;
    LinkedToken * tokens = 0;
    LinkedToken * end = 0;

    while (offset < string_len(template_string) && !error) {
        if (skip_next_whitespace) {
            while (template_string[offset] == ' ') {
                offset++;
            }
            skip_next_whitespace = false;
        }

        char c = template_string[offset];

        switch (mode) {
            case Literal:
                if (c == '$') {
                    if (escape_mode) {
                        escape_mode = false;
                        curlit = string_append(curlit, "$");
                    } else {
                        escape_mode = true;
                    }
                } else if (c == '{') {
                    if (escape_mode) {
                        if (string_len(curlit)) {
                            add_token_and_reset(TT_Str, curlit, offset, &tokens, &end);
                        }
                        mode = VarBlock;
                        skip_next_whitespace = true;
                    } else {
                        curlit = string_append(curlit, c);
                    }
                    escape_mode = false;
                } else {
                    if (escape_mode) {
                        print_error("Unexpected character (use $$ to output a literal $)", offset, offset + 1, template_string);
                        error = true;
                        break;
                    } else {
                        curlit = string_append(curlit, c);
                    }
                }
            break;

            case VarBlock:
                if (c == '}') {
                    if (string_len(curlit)) {
                        TokenType type = TT_Var;
                        if (string_eq(curlit, "else")) {
                            type = TT_Else;
                        } else if (string_eq(curlit, "end")) {
                            type = TT_End;
                        }
                        add_token_and_reset(type, curlit, offset, &tokens, &end);
                    }
                    seen_variable = false;
                    mode = Literal;
                } else if (is_identifier_char(c)) {
                    if (seen_variable) {
                        print_error("Only a single variable allowed per block", offset, offset + 1, template_string);
                        error = true;
                        break;
                    }
                    curlit = string_append(curlit, c);
                } else if (c == '?') {
                    if (string_len(curlit) > 0) {
                        add_token_and_reset(TT_If, curlit, offset, &tokens, &end);
                        skip_next_whitespace = true;
                    } else {
                        print_error("Missing variable", offset, offset + 1, template_string);
                        error = true;
                        break;
                    }
                    seen_variable = true;
                } else if (c == ' ') {
                    add_token_and_reset(TT_Var, curlit, offset, &tokens, &end);
                    seen_variable = true;
                    skip_next_whitespace = true;
                } else {
                    print_error("Unexpected character", offset, offset + 1, template_string);
                    error = true;
                    break;
                }
            break;
        }

        offset++;
    }

    if (mode == Literal) {
        add_token_and_reset(TT_Str, curlit, offset, &tokens, &end);
    } else if(!error) {
        print_error("Unfinished variable block", string_len(template_string) - 1, string_len(template_string), template_string);
        error = true;
    }

    string_free(curlit);

    if (error) {
        linked_token_free(tokens);
        return 0;
    }

    return tokens;
}

static String
get_truthy_value(VarList * vars, const char * name) {
    String value = template_get(vars, name);
    return value && !string_eq(value, "") ? value : 0;
}

static bool
_process_conditional(
        LinkedToken ** tokens,
        VarList * vars,
        String template_string,
        bool skip_all,
        String * result_out
) {
    LinkedToken * token = (*tokens);

    bool error = false;
    bool seen_else = false;
    bool seen_end = false;

    assert(token->type == TT_If);
    bool skip_block = get_truthy_value(vars, token->value) == 0;

    while ((token = token->next)) {
        skip_block = skip_all || skip_block;

        if (token->type == TT_Str) {
            if (skip_block)
                continue;
            *result_out = string_append(*result_out, token->value);
        } else if (token->type == TT_Var) {
            if (skip_block)
                continue;
            String value = get_truthy_value(vars, token->value);
            if (value) {
                *result_out = string_append(*result_out, value);
            }
        } else if (token->type == TT_If) {
            if (!_process_conditional(&token, vars, template_string, skip_block, result_out)) {
                error = true;
                break;
            }
        } else if (token->type == TT_Else) {
            if (seen_else) {
                print_error("Too many ${else} blocks", token->start, token->end, template_string);
                error = true;
                break;
            }
            seen_else = true;
            skip_block = !skip_block;
        } else if (token->type == TT_End) {
            seen_end = true;
            break;
        }
    }

    // Write the token to the caller to continue from
    *tokens = token;

    // The missing end is a special error case that we can't detect in the
    // loop. Instead we detect it by exiting the loop without any other error,
    // but without ever seening an END block.
    if (!error && !seen_end) {
        print_error("Missing ${end}", string_len(template_string) - 1, string_len(template_string), template_string);
        return false;
    }

    return !error;
}

VarList *
template_set(VarList * head, const char * varname, const char * varvalue) {
    VarList * node = head;
    VarList * end = 0;

    while (node && !string_eq(node->name, varname)) {
        end = node;
        node = node->next;
    }

    if (node) {
        // Overwrite the value of the existing variable with the same name
        node->value = string_copy(node->value, varvalue);
    } else {
        // No existing node found, create a new one and append it to the end
        node = (VarList *) calloc(1, sizeof(VarList));
        node->name = string_new(varname);
        node->value = string_new(varvalue);
        if (end) {
            end->next = node;
        }
    }

    return head ? head : node;
}

VarList *
template_merge(VarList * base, VarList * extended) {
    VarList * result = 0;
    VarList * varptr = base;
    while (varptr) {
        result = template_set(result, varptr->name, varptr->value);
        varptr = varptr->next;
    }
    varptr = extended;
    while (varptr) {
        result = template_set(result, varptr->name, varptr->value);
        varptr = varptr->next;
    }

    return result;
}

void
template_free(VarList * node) {
    while (node) {
        string_free(node->name);
        string_free(node->value);
        VarList * dead = node;
        node = node->next;
        free(dead);
    }
}

String
template_get(VarList * node, const char * name) {
    while (node) {
        if (string_eq(node->name, name)) {
            return node->value;
        }
        node = node->next;
    }
    return 0;
}

String
template_generate_usage(String template_string, const char * action_name) {
    LinkedToken * tokens = tokenize_template(template_string);
    if (!tokens) {
        return 0;
    }

    u8 seen_pos[10] = {0};
    bool has_pos_args = false;

    String named_arg_desc = string_new();
    StringList * seen_names = 0;
    bool has_named_vars = false;

    LinkedToken * tok = tokens;
    while (tok) {
        if ((tok->type == TT_If) || (tok->type == TT_Var)) {
            if (string_len(tok->value) == 1 && (is_digit(*tok->value))) {
                // Collect all of the seen positional arguments and loop over them in
                // position order afterward. They can appear in any order in the template,
                // but the order is (obviously) fixed on the command line.
                seen_pos[*tok->value - '0'] = 1;
                has_pos_args = true;
            } else {
                if (!string_list_contains(seen_names, tok->value)) {
                    named_arg_desc = string_append(named_arg_desc, " [--");
                    named_arg_desc = string_append(named_arg_desc, tok->value);
                    named_arg_desc = string_append(named_arg_desc, " <value>]");
                    seen_names = string_list_add_front_dup(seen_names, tok->value);
                    has_named_vars = true;
                }
            }
        }
        tok = tok->next;
    }
    string_list_free(seen_names);
    linked_token_free(tokens);

    String result = string_new("Usage: ");
    result = string_append(result, action_name);

    if (has_pos_args) {
        String pos_arg_desc = string_new();
        for (u8 i = 0; i < 10; i++) {
            if (seen_pos[i]) {
                pos_arg_desc = string_append(pos_arg_desc, " $");
                pos_arg_desc = string_append(pos_arg_desc, (char)('0' + i));
            }
        }
        result = string_append(result, pos_arg_desc);
        string_free(pos_arg_desc);
    }

    if (has_named_vars) {
        result = string_append(result, named_arg_desc);
        string_free(named_arg_desc);
    }

    result = string_append(result, '\n');
    return result;
}

String
template_render(String template_string, VarList * vars) {
    LinkedToken * tokens = tokenize_template(template_string);

    if (!tokens) {
        // Failed to tokenize the template string
        return 0;
    }

    String result = string_new();
    bool error = false;
    LinkedToken * curtok = tokens;
    while (curtok) {
        // printf("Token main loop: %d [%s]\n", curtok->type, curtok->value);
        if (curtok->type == TT_If) {
            if(!_process_conditional(&curtok, vars, template_string, false, &result)) {
                error = true;
                break;
            }
        } else if (curtok->type == TT_Str) {
            result = string_append(result, curtok->value);
        } else if (curtok->type == TT_Else) {
            print_error("Unexpected ${else} block", curtok->start, curtok->end, template_string);
            error = true;
            break;
        } else if (curtok->type == TT_End) {
            print_error("Unexpected ${end} block", curtok->start, curtok->end, template_string);
            error = true;
            break;
        } else if (curtok->type == TT_Var) {
            String value = get_truthy_value(vars, curtok->value);
            if (value) {
                result = string_append(result, value);
            }
        }

        curtok = curtok->next;
    }

    if (error) {
        string_free(result);
        result = 0;
    }

    linked_token_free(tokens);

    return result;
}
