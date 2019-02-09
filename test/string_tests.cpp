#include <stdio.h>
#include <assert.h>
#include "../string.h"

static void test_string_eq() {
    assert(string_eq("foobar", "foobar"));
    assert(!string_eq("foobar", "foobarr"));
    assert(!string_eq("foobar", "fooba"));
    assert(string_eq("", ""));
    char sample[] = "--argument";
    assert(string_eq((sample + 2), "argument"));
}

static void test_string_starts_with() {
    assert(string_starts_with("--argument", "--"));
    assert(string_starts_with("--argument", "-"));
    assert(!string_starts_with("--argument", ""));
    assert(!string_starts_with(0, "foobar"));
}

static void test_string_new() {
    String str = string_new("foobar");
    assert(string_eq(str, "foobar"));
    string_free(str);

    str = string_new("");
    assert(string_eq(str, ""));
    string_free(str);
}

static void test_string_copy() {
    {
        // Copy into new string
        String result = string_new();
        result = string_copy(result, "plain");
        printf("res: %s\n", result);
        assert(string_eq(result, "plain"));
        assert(string_len(result) == 5);
        string_free(result);
    }

    {
        // Copy subset of string
        String result = string_new();
        result = string_copy(result, "subset", 3);
        assert(string_eq(result, "sub"));
        assert(string_len(result) == 3);
        string_free(result);
    }

    {
        // Copy superset of string
        String result = string_new();
        result = string_copy(result, "superset", 9);
        assert(string_eq(result, "superset"));
        assert(string_len(result) == 8);
        string_free(result);
    }

    {
        // Overwrite existing string
        String result = string_new("foobar baz qux hello luisiana");
        result = string_copy(result, "overwrite");
        assert(string_eq(result, "overwrite"));
        assert(string_len(result) == 9);
        string_free(result);
    }
}

static void test_string_len() {
    {
        String string = string_new();
        assert(string_len(string) == 0);
        set_string_len(string, 258);
        assert(string_len(string) == 258);
        string_free(string);
    }

    {
        String string = string_new("foobar");
        assert(string_len(string) == 6);
        string_free(string);
    }
}


int main() {
    test_string_eq();
    test_string_starts_with();
    test_string_new();
    test_string_copy();
    test_string_len();
}
