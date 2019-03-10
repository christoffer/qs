#!/usr/bin/env python3

from test_framework import *

@test
def basic_cli():
    run('--version').and_expect(stdout='1.1.0')
    run().and_expect(exit_code=2, stdout_regex='^Usage:')

    # Argument errors
    run('--foo', 'bar').and_expect(exit_code=2, stdout_regex=r'^Error: Must provide either an action name or a --template.*')
    run('foobar', '--template', 'echo Hello').and_expect(
         exit_code=2,
         stdout='Error: Must provide either an action name or a template string (--template), not both.'
    )
    run('dummy', '--missing').and_expect(exit_code=2, stdout="Missing value for variable 'missing'")
    run('non-existing-action', '--config', 'missing-file').and_expect(
        exit_code=2,
        stdout_regex=r"Warning: could not read the config file 'missing-file'. Ignoring.",
    )
    # test('--template: empty',
    run('--template').and_expect(
         exit_code=2,
         stdout='--template should be followed by a template string.'
    )

@test
def template_arg_substition(env_root):
    run('--template', 'echo "it works!"').and_expect(stdout="it works!")
    run('--dry-run', '--template', 'echo "it would work!"').and_expect(stdout='Would run: cd .; QS_RUN_DIR=%s; echo "it would work!"' % env_root)

    run('--template', 'echo "Hello ${name}!"', '--name', 'Christoffer').and_expect(stdout='Hello Christoffer!')
    run('--template', 'echo "Hello ${ name }!"', '--name', 'Christoffer').and_expect(stdout='Hello Christoffer!')
    run('--template', 'echo "Hello ${prefix}${name}!"', '--name', 'Christoffer', '--prefix', 'Mr.').and_expect(stdout='Hello Mr.Christoffer!')
    run(
        '--template',
        'echo "a:${a?}set${else}unset${end} b:${b?}set${else}unset${end} c:${c?}set${else}unset${end}"',
        '--a', '',
        '--b', 'yes'
    ).and_expect(stdout='a:unset b:set c:unset')
    run('--template', 'echo "EnvVar: $$MYENV"', env={'MYENV': 'foobar'}).and_expect(stdout='EnvVar: foobar')

@test
def template_error_handling():
    run('--template', 'echo "Invalid: ${ invalid block }"').and_expect(
         exit_code=2,
         stdout=(
             'Error: Only a single variable allowed per block.\n'
             'echo "Invalid: ${ invalid block }"\n'
             '                          ^'
         )
    )
    run('--template', 'echo "${name?}name"').and_expect(
         exit_code=2,
         stdout=(
             'Error: Missing ${end}.\n'
             'echo "${name?}name"\n'
             '                  ^'
         )
    )
    run('--template', 'echo "${name}name${end}"').and_expect(
         exit_code=2,
         stdout=(
             'Error: Unexpected ${end} block.\n'
             'echo "${name}name${end}"\n'
             '                   ^^^'
         )
    )
    run('--template', 'echo "${@&@!^(%!}').and_expect(
        exit_code=2,
        stdout=(
            'Error: Unexpected character.\n'
            'echo "${@&@!^(%!}\n'
            '        ^'
        )
    )
    run('--template', 'echo "${else}name${end}"').and_expect(
         exit_code=2,
         stdout=(
             'Error: Unexpected ${else} block.\n'
             'echo "${else}name${end}"\n'
             '        ^^^^'
         )
    )
    run('--template', '${var?}1${else}2${else}3${end}').and_expect(
         exit_code=2,
         stdout=(
             'Error: Too many ${else} blocks.\n'
             '${var?}1${else}2${else}3${end}\n'
             '                  ^^^^'
         )
    )
    run('--template', 'echo "$MYENV"').and_expect(
         exit_code=2,
         stdout=(
             'Error: Unexpected character (use $$ to output a literal $).\n'
             'echo "$MYENV"\n'
             '       ^'
         ),
    )
    run('--template', 'echo "${  ?  }"').and_expect(
         exit_code=2,
         stdout=(
             'Error: Missing variable.\n'
             'echo "${  ?  }"\n'
             '          ^'
         ),
    )
    run('--template', '${var?').and_expect(
         exit_code=2,
         stdout=(
             'Error: Unfinished variable block.\n'
             '${var?\n'
             '     ^'
         )
    )

@test({
    ".git/config": '',
    ".qs.cfg": 'make=echo "make in source root"\nreload=echo "reload in root"',
    "src/.qs.cfg": 'make=echo "make local"',
})
def resolve_source_root_config(root):
    run('make', run_from_dir='src').and_expect(stdout='make local')
    run('reload', run_from_dir='src').and_expect(stdout='reload in root')

@test({
    '.qs.cfg': '''
        local = echo "foo local"
        custom = echo "boop local"
    ''',
    'config/qs/default.cfg': '''
        local = echo "foo local"
        custom = echo "boop local"
        default = echo "qux default"
    ''',
    'custom.cfg': '''
        custom = echo "boop overwrite"
    ''',
})
def show_available_commands(root):
    xdg_config_home = os.path.join(root, 'config')
    env={'XDG_CONFIG_HOME': xdg_config_home}
    run('--actions', '--config', 'custom.cfg', env=env).and_expect(
        stdout = (
            'Available actions:\n'
            ' - custom                              ({0}/custom.cfg)\n'
            ' - local                               ({0}/.qs.cfg)\n'
            ' - default                             ({0}/config/qs/default.cfg)'
        ).format(root)
    )

@test({"custom.cfg": 'predefined=echo "Predefined in custom cfg works!"'})
def resolve_action_in_custom_config():
    run('predefined', '--config', 'custom.cfg').and_expect(stdout='Predefined in custom cfg works!')

# TODO - duplicate action names

@test({'a': 'cmd=echo "a"', 'b': 'cmd=echo "b"'})
def custom_config_priority():
    run('cmd', '--config', 'a', '--config', 'b').and_expect(stdout='b')

@test({
    'incomplete': 'action',
    'missing-action-value': 'action=',
    'missing-arg-value': 'arg:=',
    'misplaced-comment-action': 'action = # comment',
    'misplaced-comment-arg': 'arg := # comment',
    'weird-char': '!foo = bar',
})
def broken_config_file_handling(root):
    def do_test(filename, expected_message):
        message_regex=r'Error in %s/%s: %s' % (root, filename, expected_message)
        run('action', '--config', filename).and_expect(exit_code=1, stderr_regex=message_regex)

    do_test('incomplete', "Expected '=' or ':='")
    do_test('missing-action-value', "No value after '='")
    do_test('missing-arg-value', "No value after ':='")
    do_test('misplaced-comment-action', "Action template cannot start with '#'")
    do_test('misplaced-comment-arg', "Argument value cannot start with '#'")
    do_test('weird-char', "Unexpected character '!' \(33\)")

@test({'custom-configs/qs/default.cfg': 'custom=echo "resolved custom xdg config"'})
def xdg_home_resolution(env):
    test_home = os.path.join(env, 'home')
    xdg_config_home = os.path.join(env, 'custom-configs')

    run('custom', env={'XDG_CONFIG_HOME': xdg_config_home, 'HOME': test_home}).and_expect(
        stdout='resolved custom xdg config'
    )
    run('custom', env={'XDG_CONFIG_HOME': xdg_config_home + '/', 'HOME': test_home}).and_expect(
         stdout='resolved custom xdg config'
    )
    run('custom', env={'XDG_CONFIG_HOME': '0;42;\t \n 4-43', 'HOME': test_home}).and_expect(
        exit_code=2, stdout='Could not find action with name: custom'
    )

@test({
    'home/.config/qs/default.cfg': 'default=echo "resolved default xdg config"',
})
def resolve_default_xdg_home_resolution(env):
    test_home = os.path.join(env, 'home')
    run('default', env={'HOME': test_home}).and_expect(stdout='resolved default xdg config')
    run('default', env={'HOME': test_home + '/'}).and_expect(stdout='resolved default xdg config')
    run('default', env={'HOME': '\t \n\u2528-43'}).and_expect(exit_code=2, stdout='Could not find action with name: default')

@test({'.qs.cfg': 'cmd=echo "${0} and ${1}"'})
def positional_arguments():
    run('cmd', 'foo', 'bar').and_expect(stdout='foo and bar')
    run('cmd', 'var1', 'var2', 'var3', 'var4', 'var5', 'var6', 'var7', 'var8', 'var9', 'var10', 'var11').and_expect(
        exit_code=2,
        stdout_regex='At most 10 positional arguments can be given',
    )

@test({
    'mixed.cfg': 'foo=n${name}l${lname} ${0}',
    'multi.cfg': 'foo=n${name}l${name}${lname}/${ name } ${1}${0}${0}',
})
def action_help_string():
    run('foo', '--help', '--config', 'mixed.cfg').and_expect(
        exit_code=0,
        stdout='Usage: foo $0 [--name <value>] [--lname <value>]',
    )
    run('foo', '--help', '--config', 'multi.cfg').and_expect(
        exit_code=0,
        stdout='Usage: foo $0 $1 [--name <value>] [--lname <value>]',
    )

@test({
    '.qs.cfg': '''
       flags := --foo bar
       cmd=command ${flags}
    '''
})
def default_args_in_configs():
    run('cmd', '--dry-run').and_expect(stdout_regex=r"Would run: .* command --foo bar")
    run('cmd', '--dry-run', '--flags', '/overwrite').and_expect(stdout_regex=r"Would run: .* command /overwrite")

@test({
    'source/root/.git/config': '', # Define source root
    'source/root/.qs.cfg': 'print-cwd=pwd',
    'source/root/src/files/file.c': 'this is here to make the directory exist',
    'a/b/ab.cfg': 'print-cwd=pwd',
    '.qs.cfg': 'print-cwd=pwd',
})
def action_cwd(root):
    # Pointing to a custom config should resolve from that config file
    run('print-cwd', '--config', 'a/b/ab.cfg').and_expect(stdout='%s/a/b' % root)
    # Running from the test root should use the cwd
    run('print-cwd').and_expect(stdout='%s' % root)
    # Running an action in the source root config should use the source root as cwd
    run('print-cwd', run_from_dir='source/root/src/files').and_expect(stdout='%s/source/root' % root)

@test({
    'workdir/file': '',
    '.git/config': '',
    '.qs.cfg': 'cmd=echo "\$$QS_RUN_DIR=$$QS_RUN_DIR"',
})
def set_qs_run_dir(env):
    run('cmd', run_from_dir='workdir').and_expect(stdout='$QS_RUN_DIR=%s/workdir' % env)

run_tests_and_report()
