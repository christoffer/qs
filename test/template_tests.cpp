#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../string.h"
#include "../templates.h"

static void assertstr(char* actual, const char* expected)
{
    assert(actual);
    if (!string_eq(actual, expected)) {
        fprintf(stdout, "Assertion! Expected: [%s], got [%s]\n", expected, actual);
        exit(1);
    }
}

static u32 varlist_size(VarList* vl)
{
    VarList* p = vl;
    u32 s = 0;
    while (p) {
        s++;
        p = p->next;
    }
    return s;
}

static void test_template_set()
{
    VarList* vars = template_set(0, "first", "one");

    assert(vars);
    assert(string_eq(vars->name, "first"));
    assert(string_eq(vars->value, "one"));
    assert(vars->next == 0);
    assert(varlist_size(vars) == 1);

    vars = template_set(vars, "second", "two");
    assertstr(vars->name, "first");
    assertstr(vars->next->name, "second");
    assert(varlist_size(vars) == 2);

    vars = template_set(vars, "first", "overwritten");
    assertstr(vars->value, "overwritten");
    assertstr(vars->next->name, "second");
    assert(vars->next->next == 0);
    assert(varlist_size(vars) == 2);

    template_free(vars);
}

static void test_template_get()
{
    // VarList * vars = template_set(0, "name", "Christoffer");
    // vars = template_set(vars, "empty", "");
    VarList* vars = template_set(0, "empty", "");

    // assertstr(template_get(vars, "name"), "Christoffer");
    assertstr(template_get(vars, "empty"), "");
    assert(!template_get(vars, "missing"));

    template_free(vars);
}

static void test_basic_render()
{
    String action_template = string_new("hello ${name} ${   lastname    }!");

    VarList* vars = 0;
    vars = template_set(vars, "name", "Christoffer");
    vars = template_set(vars, "lastname", "Klang");

    String result = template_render(action_template, vars);
    assertstr(result, "hello Christoffer Klang!");
    string_free(action_template);
    template_free(vars);
    string_free(result);
}

static void test_conditionals_basic()
{
    String action_template = string_new("${name?}Hello ${name}${else}Hi!${end}");
    VarList* vars = 0;

    String result;

    result = template_render(action_template, vars);
    assertstr(result, "Hi!");
    string_free(result);

    vars = template_set(vars, "name", "Christoffer");
    result = template_render(action_template, vars);
    assertstr(result, "Hello Christoffer");
    string_free(result);

    vars = template_set(vars, "name", "");
    result = template_render(action_template, vars);
    assertstr(result, "Hi!");
    string_free(result);

    template_free(vars);
    string_free(action_template);
}

static void test_conditionals_nested()
{
    String action_template = string_new("${a?}${b?}a&b${else}a&!b${end}${else}${b?}!a&b${else}!a&!b${end}${end}");
    VarList* vars = 0;

    String result;

    result = template_render(action_template, vars);
    assertstr(result, "!a&!b");
    string_free(result);

    vars = template_set(vars, "a", "a");
    result = template_render(action_template, vars);
    assertstr(result, "a&!b");
    string_free(result);

    vars = template_set(vars, "b", "b");
    result = template_render(action_template, vars);
    assertstr(result, "a&b");
    string_free(result);

    vars = template_set(vars, "a", "");
    result = template_render(action_template, vars);
    assertstr(result, "!a&b");
    string_free(result);

    template_free(vars);
    string_free(action_template);
}

static void test_template_generate_usage()
{
    String action_template = string_new("something ${0} and then ${name}, and then ${something}, and finally ${1}");
    String result = template_generate_usage(action_template, "foobar");
    assertstr(result, "Usage: foobar $0 $1 [--name <value>] [--something <value>]\n");

    string_free(result);
    string_free(action_template);
}

static void test_template_merge()
{
    {
        VarList* a = 0;
        a = template_set(a, "foo", "a");
        a = template_set(a, "bar", "a");
        VarList* b = 0;
        a = template_set(a, "qux", "b");
        a = template_set(a, "bar", "b");

        VarList* result = template_merge(a, b);
        assertstr(template_get(result, "foo"), "a");
        assertstr(template_get(result, "bar"), "b");
        assertstr(template_get(result, "qux"), "b");

        template_free(a);
        template_free(b);
        template_free(result);
    }

    {
        VarList* a = template_set(0, "key", "value");

        VarList* result = template_merge(a, 0);
        assertstr(template_get(a, "key"), "value");
        template_free(result);

        result = template_merge(0, a);
        assertstr(template_get(a, "key"), "value");

        template_free(result);
        template_free(a);
    }
}

int main()
{
    test_template_set();
    test_template_get();
    test_basic_render();
    test_conditionals_basic();
    test_conditionals_nested();
    test_template_generate_usage();
    test_template_merge();
}
