#pragma once
#include "base.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-macros"

#define is_alpha(c) ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'))
#define is_digit(c) (c >= '0' && c <= '9')
#define is_identifier_char(c) (is_alpha(c) || is_digit(c) || c == '-' || c == '_')

// NOTE(christoffer) Cast to void * to avoid the alignment warning from clang.
// It seems like the x86 architecture doesn't have an aligment requirement on int (vs char)
// which the warning is warning about, so I think we're fine until we actually want to support
// different architectures.
#define string_len(string) (*((u32 *)(void *)(string - 4)))
#define set_string_len(string, len) (*((u32 *)(void *)(string - 4)) = len)

#pragma clang diagnostic pop

typedef char * String;

struct StringList {
    String string = 0;
    StringList * next = 0;
};

String string_new();
String string_new(const char * content);

void string_clear(String string);
void string_free(String string);
String string_resizebuf(String string, u32 new_bufsize);
String string_ensure_fits_len(String string, u32 at_least_length);
String string_append(String string, const char * content);
String string_append(String string, const char chr);
String string_copy(String string, const char * content, u32 count);
String string_copy(String string, const char * content);

StringList * string_list_add_front_dup(StringList * list, const char *);
bool string_list_contains(StringList * list, const char *);
void string_list_free(StringList * list);

bool string_eq(const char * a, const char * b);
bool string_starts_with(const char * string, const char * substring);

u32 cstrlen(const char * cstr);
void cstrcpy(char * dest, const char * src);
void cstrcat(char * dest, const char * src);
