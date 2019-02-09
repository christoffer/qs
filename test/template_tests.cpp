#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "../string.h"
#include "../templates.h"

static void assertstr(char * actual, const char * expected) {
    assert(actual);
    if (!string_eq(actual, expected)) {
        fprintf(stdout, "Assertion! Expected: [%s], got [%s]\n", expected, actual);\
        exit(1);
    }
}

static void test_template_set() {
    VarList * vars = template_set(0, "first", "one");

    assert(vars);
    assert(string_eq(vars->name, "first"));
    assert(string_eq(vars->value, "one"));
    assert(vars->next == 0);

    vars = template_set(vars, "second", "two");
    assertstr(vars->name, "first");
    assertstr(vars->next->name, "second");

    vars = template_set(vars, "first", "overwritten");
    assertstr(vars->value, "overwritten");
    assertstr(vars->next->name, "second");
    assert(vars->next->next == 0);

    template_free(vars);
}

static void test_template_get() {
    // VarList * vars = template_set(0, "name", "Christoffer");
    // vars = template_set(vars, "empty", "");
    VarList * vars = template_set(0, "empty", "");

    // assertstr(template_get(vars, "name"), "Christoffer");
    assertstr(template_get(vars, "empty"), "");
    assert(!template_get(vars, "missing"));

    template_free(vars);
}

// static void test_basic_render() {
//     String template_string = string_new("hello ${name} ${   lastname    }!");
//
//     VariableSet vars = {};
//     template_set(&vars, "name", "Christoffer");
//     template_set(&vars, "lastname", "Klang");
//
//     String result = template_render(template_string, vars);
//     assertstr(result, "hello Christoffer Klang!");
//     string_free(template_string);
// }
//
// static void test_conditionals_basic() {
//     String template_string = string_new("${name?}Hello ${name}${else}Hi!${end}");
//     VariableSet vars = {};
//
//     String result;
//
//     result = template_render(template_string, vars);
//     assertstr(result, "Hi!");
//
//     template_set(&vars, "name", "Christoffer");
//     result = template_render(template_string, vars);
//     assertstr(result, "Hello Christoffer");
//
//     template_set(&vars, "name", "");
//     result = template_render(template_string, vars);
//     assertstr(result, "Hi!");
// }
//
// static void test_conditionals_nested() {
//     String template_string = string_new("${a?}${b?}a&b${else}a&!b${end}${else}${b?}!a&b${else}!a&!b${end}${end}");
//     VariableSet vars = {};
//     String result;
//
//     result = template_render(template_string, vars);
//     assertstr(result, "!a&!b");
//
//     template_set(&vars, "a", "a");
//     template_render(template_string, vars);
//     result = template_render(template_string, vars);
//     assertstr(result, "a&!b");
//
//     template_set(&vars, "b", "b");
//     template_render(template_string, vars);
//     result = template_render(template_string, vars);
//     assertstr(result, "a&b");
//
//     template_set(&vars, "a", "");
//     template_render(template_string, vars);
//     result = template_render(template_string, vars);
//     assertstr(result, "!a&b");
//
//     string_free(template_string);
// }

int main() {
    test_template_set();
    test_template_get();
    // test_basic_render();
    // test_conditionals_basic();
    // test_conditionals_nested();
}
