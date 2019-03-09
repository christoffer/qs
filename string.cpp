#include <stdlib.h>
#include <assert.h>

#include "base.h"
#include "string.h"

// Header size in bytes (4 for bufsize, 4 for string length)
#define HEADER_SIZE 8
#define NUL_SIZE 1

// NOTE(christoffer) See note about casting to void * in string.h
#define _get_bufsize(str) (*((u32*)((void *)(str - HEADER_SIZE))))
#define _set_bufsize(str, bufsize) (*((u32 *)(void *)((str - HEADER_SIZE))) = bufsize)

/**
 * A String implementation where the buffer size and the string length are prepended to the
 * content buffer.
 *
 * These functions all return a pointer to the content, so that it can be used with any
 * other function that expects a cstring.
 *
 * [ buffer_size (4 bytes) ][ string length (4 bytes) ][ content (buffer_size bytes)
 *                                                     ^  pointer returned to caller
 */

u32
cstrlen(const char * cstr) {
    u32 nul_offset = 0;
    while (*(cstr + nul_offset++));
    return nul_offset - 1;
}

void
cstrcpy(char * dest, const char * src) {
    while((*(dest++) = *(src++)));
}

void
cstrcat(char * dest, const char * src) {
    while(*(dest++));                // seek to %nul
    dest--;                          // backtrack to overwrite the %nul
    while((*(dest++) = *(src++)));   // copy until (including) src %nul
}

String
string_new() {
    u32 bufsize = HEADER_SIZE + NUL_SIZE;
    char * buf = (char *) calloc(1, bufsize);
    assert(buf);
    String string = (String)(buf + HEADER_SIZE);
    _set_bufsize(string, bufsize);
    string_clear(string);
    return string;
}

String
string_new(const char * content) {
    u32 content_len = cstrlen(content);
    u32 req_bufsize = HEADER_SIZE + content_len + NUL_SIZE;
    char * buf = (char *) calloc(1, req_bufsize);
    assert(buf);

    String string = (String)(buf + HEADER_SIZE);
    cstrcpy(string, content);
    *(string + content_len) = '\0';

    _set_bufsize(string, req_bufsize);
    set_string_len(string, content_len);
    return string;
}

void
string_clear(String string) {
    set_string_len(string, 0);
    *string = '\0';
}

void
string_free(String string) {
    if (string) {
        char * bufptr = ((char *) string) - HEADER_SIZE;
        free(bufptr);
    }
}

String
string_resizebuf(String string, u32 new_bufsize) {
    char * bufptr = string - HEADER_SIZE;
    u32 cur_bufsize = _get_bufsize(string);

    if (new_bufsize == cur_bufsize) {
        // Optimization, noop if we don't need resize
        return string;
    }

    // Resize buffer and write the new buffer size
    bufptr = (char *) realloc(bufptr, new_bufsize);
    assert(bufptr);
    string = bufptr + HEADER_SIZE;
    _set_bufsize(string, new_bufsize);

    // If we shrunk the buffer, make sure that it ends with a %nul and that the new length is corrected
    if (new_bufsize < cur_bufsize) {
        u32 new_len = new_bufsize - HEADER_SIZE - NUL_SIZE;
        *(string + new_len) = '\0';
        set_string_len(string, new_len);
    }

    return string;
}

String
string_ensure_fits_len(String string, u32 at_least_length) {
    u32 curlen = string_len(string);
    if (curlen < at_least_length) {
        u32 req_bufsize = at_least_length + HEADER_SIZE + NUL_SIZE;
        string = string_resizebuf(string, req_bufsize);
    }
    return string;
}

String
string_append(String string, const char * content) {
    u32 cur_len = string_len(string);
    u32 new_len = cur_len + cstrlen(content);
    string = string_ensure_fits_len(string, new_len);
    cstrcat(string, content);
    *(string + new_len) = '\0';
    set_string_len(string, new_len);
    return string;
}
String
string_append(String string, const char chr) {
    u32 cur_len = string_len(string);
    u32 new_len = cur_len + 1;
    string = string_ensure_fits_len(string, new_len);

    string[cur_len] = chr;
    string[new_len] = '\0';
    set_string_len(string, new_len);
    return string;
}

String
string_copy(String string, const char * content, u32 count) {
    u32 content_len = cstrlen(content);
    u32 new_len = content_len < count ? content_len : count;
    string = string_ensure_fits_len(string, new_len);

    u32 copied_len = 0;
    char * dest = string;
    while (*content && (copied_len < new_len)) {
        *(dest++) = *(content++);
        copied_len++;
    }
    *dest = '\0';

    set_string_len(string, copied_len);
    return string;
}

String
string_copy(String string, const char * content) {
    u32 bytelimit = cstrlen(content);
    String result = string_copy(string, content, bytelimit);
    return result;
}

StringList *
string_push_dup_front(StringList * list, const char * content) {
    StringList * node = (StringList *) calloc(1, sizeof(StringList));
    node->string = string_new(content);
    node->next = list;
    return node;
}

bool
string_list_contains(StringList * list, const char * value) {
    while(list) {
        if (string_eq(list->string, value)) {
            return true;
        }
        list = list->next;
    }
    return false;
}

void
string_list_free(StringList * list) {
    while(list) {
        StringList * dead = list;
        list = list->next;
        string_free(dead->string);
        free(dead);
    }
}

bool
string_eq(const char * a, const char * b) {
    // Break immidiately if we didn't get strings, or if the first char doesn't match
    if ((!a || !b) || (*a != *b)) {
        return false;
    }
    // Loop while both of the strings are equal, and both of them have a value.
    while((*a && *b) && (*a == *b)) {
        a++;
        b++;
    }
    // The only case where the strings are equal is if they're both depleted at the same time
    return !(*a || *b);
}

bool
string_starts_with(const char * string, const char * substring) {
    if (!string || !substring) {
        return false;
    }

    do {
        if (*string != *substring) {
            return false;
        }
    } while (*(++substring) && *(++string));
    return true;
}

