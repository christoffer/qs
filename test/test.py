#!/usr/bin/env python3

import subprocess
from contextlib import contextmanager
import os
import re
import tempfile
import shutil
import errno

CRED = '\033[91m'
CGREEN = '\033[92m'
CREDBG = '\x1b[6;39;41m'
CGREENBG = '\x1b[6;30;42m'
CBLUE = '\033[94m'
CYELLOW = '\033[93m'
CEND = '\033[0m'

source_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

@contextmanager
def test_env(filetree={}, shell_env={}):
    tmpdir = tempfile.mkdtemp(prefix="qs-test-")
    try:
        for rel_path in filetree.keys():
            abs_path = os.path.abspath(os.path.join(tmpdir, rel_path))
            assert abs_path.startswith(tmpdir)
            rel_dir = os.path.dirname(abs_path)
            subprocess.check_call(['mkdir', '-p', rel_dir])
            with open(abs_path, 'w') as f:
                f.write(filetree[rel_path])
        yield tmpdir
    finally:
        shutil.rmtree(tmpdir)

def build():
    print(subprocess.check_call(['make debug'], shell=True, cwd=source_root))
    print(f"\n{CYELLOW}Rebuilding: OK!{CEND}")

def std_stream_err(stream_name, expected, actual):
    return f"{CRED} \u2717 Unexpected {stream_name}{CEND}:\nGot:\n[{CEND}{actual}]{CEND}\nExpected:\n[{CEND}{expected}]{CEND}"

num_run = 0
num_failed = 0
num_skipped = 0
error_reports = []

def run_test(env_root,
             name,
             args,
             exit_code=0,
             stdout=None, stderr=None,
             stdout_regex=None,
             stderr_regex=None,
             run_from_dir=None,
             **kwargs):
    global num_run, num_failed, error_reports, only_run

    shutil.copy(os.path.join(source_root, 'bin/qs'), os.path.join(env_root))
    cwd = os.path.join(env_root, run_from_dir) if run_from_dir else env_root
    binary = os.path.join(env_root, 'qs')

    num_run += + 1
    result = subprocess.run([binary, *args], stdout=subprocess.PIPE, stderr=subprocess.PIPE, cwd=cwd, **kwargs)
    [actual_exit, actual_stdout, actual_stderr] = (result.returncode, result.stdout.decode('utf-8').strip(), result.stderr.decode('utf-8').strip())

    errors = []

    if exit_code != actual_exit:
        errors.append(f"\n{CRED} \u2717 Unexpected exit code{CEND}: Expected: {exit_code}, got: {actual_exit}")
    if stdout_regex is not None and not re.match(stdout_regex, actual_stdout, re.MULTILINE | re.DOTALL):
        errors.append(std_stream_err('stdout (regex)', stdout_regex, actual_stdout))
    if stdout is not None and stdout != actual_stdout:
        errors.append(std_stream_err('stdout', stdout, actual_stdout))
    if stderr is not None and stderr != actual_stderr:
        errors.append(std_stream_err('stderr', stderr, actual_stderr))
    if stderr_regex is not None and not re.match(stderr_regex, actual_stderr, re.MULTILINE | re.DOTALL):
        errors.append(std_stream_err('stderr (regex)', stderr_regex, actual_stderr))

    if len(errors) > 0:
        num_failed = num_failed + 1
        print(f"{CREDBG}\u2717 Failed {CEND}{CRED} Test: [{name}]{CEND}")
        error_reports += [
            f"{CRED}----------------------------------------{CEND}",
            f"{CREDBG}\u2717 Failed {CEND}{CRED} Test: [{name}]{CEND}",
            f"{CRED}----------------------------------------{CEND}",
            "Execution information:",
            f"command: {CBLUE}./bin/qs %s{CEND}" % ' '.join(map((lambda x: "'%s'" % x), args)),
            f"stdout:\n{actual_stdout}{CEND}\nstderr:\n{actual_stderr}{CEND}",
            f"{CRED}Test failure(s):{CEND}",
            '\n'.join(errors),
            f"{CRED}----------------------------------------{CEND}",
            "\n\n"
        ]
    else:
        print(f"{CGREENBG}\u2713 Passed {CEND} {CGREEN}Test: [{name}] {CEND}")


# test-start

build()

# Basic CLI stuff
with test_env({}) as env_root:
    def test(*args, **kwargs):
        run_test(env_root, *args, **kwargs)

    test('show version',
         ['--version'],
         stdout="1.0.0"
    )
    test('usage on no args',
         [],
         exit_code=2,
         stdout_regex=r'Usage:.*/qs \[flags\] <action name> | --template <template string> \[variables\]'
    )

    # Argument errors
    test('cli: action and --template',
         ['foobar', '--template', 'echo Hello'],
         exit_code=2,
         stdout='Error: Must provide either an action name or a template string (--template), not both.'
    )
    test('cli: missing var value',
         ['dummy', '--missing'],
         exit_code=2,
         stdout="Missing value for variable 'missing'"
    )
    test('cli: only variable',
         ['--foo', 'bar'],
         exit_code=2,
         stdout='Must provide either an action name or a --template'
    )
    test('cli: gave --config missing file',
        ['non-existing-action', '--config', 'missing-file'],
        exit_code=2,
        stdout_regex=r"Warning: could not read the config file 'missing-file'. Ignoring.",
    )
    test('--template: empty',
         ['--template'],
         exit_code=2,
         stdout='--template should be followed by a template string.'
    )

    # --template
    test('--template',
         ['--template', 'echo "it works!"'],
         stdout=r"it works!"
    )
    test('--template dry run',
         ['--dry-run', '--template', 'echo "it would work!"'],
         stdout='Would run: cd .; QS_RUN_DIR=%s; echo "it would work!"' % env_root,
    )
    test('--template var subst',
         ['--template', 'echo "Hello ${name}!"', '--name', 'Christoffer'],
         stdout='Hello Christoffer!'
    )
    test('template: basic',
         ['--template', 'echo "Hello ${name}!"', '--name', 'Christoffer'],
         stdout='Hello Christoffer!'
    )
    test('template: basic (spaces)',
         ['--template', 'echo "Hello ${ name }!"', '--name', 'Christoffer'],
         stdout='Hello Christoffer!'
    )
    test('template: basic (narrow)',
         ['--template', 'echo "Hello ${prefix}${name}!"', '--name', 'Christoffer', '--prefix', 'Mr.'],
         stdout='Hello Mr.Christoffer!'
    )

    # Template conditionals
    test('template: if',
        [
            '--template',
            'echo "a:${a?}set${else}unset${end} b:${b?}set${else}unset${end} c:${c?}set${else}unset${end}"',
            '--a', '', '--b', 'yes',
        ],
        stdout='a:unset b:set c:unset'
    )

    # Template error handling
    test('template: invalid var block',
         ['--template', 'echo "Invalid: ${ invalid block }"'],
         exit_code=2,
         stdout=(
             'Error: Only a single variable allowed per block.\n'
             'echo "Invalid: ${ invalid block }"\n'
             '                          ^'
         )
    )
    test('template: missing end',
         ['--template', 'echo "${name?}name"'],
         exit_code=2,
         stdout=(
             'Error: Missing ${end}.\n'
             'echo "${name?}name"\n'
             '                  ^'
         )
    )
    test('template: orphan end',
         ['--template', 'echo "${name}name${end}"'],
         exit_code=2,
         stdout=(
             'Error: Unexpected ${end} block.\n'
             'echo "${name}name${end}"\n'
             '                   ^^^'
         )
    )
    test('template: garbage', ['--template', 'echo "${@&@!^(%!}'],
         exit_code=2,
         stdout=(
             'Error: Unexpected character.\n'
             'echo "${@&@!^(%!}\n'
             '        ^'
         )
    )
    test('template: orphan else',
         ['--template', 'echo "${else}name${end}"'],
         exit_code=2,
         stdout=(
             'Error: Unexpected ${else} block.\n'
             'echo "${else}name${end}"\n'
             '        ^^^^'
         )
    )
    test('template: multiple else',
         ['--template', '${var?}1${else}2${else}3${end}'],
         exit_code=2,
         stdout=(
             'Error: Too many ${else} blocks.\n'
             '${var?}1${else}2${else}3${end}\n'
             '                  ^^^^'
         )
    )
    test('template: escaped $',
         ['--template', 'echo "EnvVar: $$MYENV"'],
         stdout='EnvVar: foobar',
         env = {'MYENV': 'foobar'},
    )
    test('template: unfinished escape seq',
         ['--template', 'echo "$MYENV"'],
         exit_code=2,
         stdout=(
             'Error: Unexpected character (use $$ to output a literal $).\n'
             'echo "$MYENV"\n'
             '       ^'
         ),
    )
    test('template: conditional no variable',
         ['--template', 'echo "${  ?  }"'],
         exit_code=2,
         stdout=(
             'Error: Missing variable.\n'
             'echo "${  ?  }"\n'
             '          ^'
         ),
    )
    test('template: unfinished variable block',
         ['--template', '${var?'],
         exit_code=2,
         stdout=(
             'Error: Unfinished variable block.\n'
             '${var?\n'
             '     ^'
         )
    )

# Config files
with test_env({"custom.cfg": 'predefined=echo "Predefined in custom cfg works!"'}) as env:
    run_test(env, '--config custom',
        ['predefined', '--config', 'custom.cfg'],
        stdout='Predefined in custom cfg works!'
    )

with test_env({'a.cfg': 'cmd=echo "a"', 'b.cfg': 'cmd=echo "b"'}) as env:
    run_test(env, '--config prio',
        ['cmd', '--config', 'a.cfg', '--config', 'b.cfg'],
        stdout='b',
    )

with test_env({'.qs.cfg': 'borked'}) as env:
    run_test(env, 'invalid config (incomplete)',
        ['broken'], exit_code=1, stderr_regex=r"Error in (.*?)/.qs.cfg: Expected '='"
    )

with test_env({
    ".git/config": '',
    ".qs.cfg": 'make=echo "make in source root"\nreload=echo "reload in root"',
    "src/.qs.cfg": 'make=echo "make local"',
    }) as env:
    run_test(env, 'source root: local preferred', ['make'],
        stdout='make local',
        run_from_dir="src"
    )
    run_test(env, 'source root: resolved', ['reload'],
        stdout='reload in root',
        run_from_dir="src"
    )

with test_env({
    'custom-configs/qs/default.cfg': 'custom=echo "resolved custom xdg config"',
}) as env:
    test_home = os.path.join(env, 'home')
    xdg_config_home = os.path.join(env, 'custom-configs')

    run_test(env, 'resolve custom xdg config home', ['custom'],
        env={'XDG_CONFIG_HOME': xdg_config_home, 'HOME': test_home},
        stdout='resolved custom xdg config',
    )
    run_test(env, 'resolve custom xdg config home (trailing slash)', ['custom'],
        env={'XDG_CONFIG_HOME': xdg_config_home + '/', 'HOME': test_home},
         stdout='resolved custom xdg config',
    )
    run_test(env, 'resolve custom xdg config home (invalid value)', ['custom'],
        env={'XDG_CONFIG_HOME': '0;42;\t \n 4-43', 'HOME': test_home},
        exit_code=2, stdout='Could not find action with name: custom',
    )

with test_env({
    'home/.config/qs/default.cfg': 'default=echo "resolved default xdg config"',
}) as env:
    test_home = os.path.join(env, 'home')

    run_test(env, 'resolve default xdg config home', ['default'],
        env={'HOME': test_home},
         stdout='resolved default xdg config',
    )
    run_test(env, 'resolve default xdg config home (trailing slash)', ['default'],
        env={'HOME': test_home + '/'},
         stdout='resolved default xdg config',
    )
    run_test(env, 'resolve default xdg config home (invalid value)', ['default'],
        env={'HOME': '0;42;\t \n4-43'},
        exit_code=2, stdout='Could not find action with name: default',
    )

with test_env({'.qs.cfg': 'cmd =  # error, no comment allowed here'}) as env:
    run_test(env, 'errors with comment values',
        ['cmd'],
        exit_code=1,
        stderr_regex=r"Error in (.*)\.qs\.cfg: Value cannot start with '#'"
    )

with test_env({'.qs.cfg': 'cmd=echo "${0} and ${1}"'}) as env:
    run_test(env, 'positional arguments',
        ['cmd', 'foo', 'bar'],
        stdout='foo and bar',
    )

with test_env({'.qs.cfg': 'cmd=echo "${0} and ${1}"'}) as env:
    run_test(env, 'max pos args',
        ['cmd', 'var1', 'var2', 'var3', 'var4', 'var5', 'var6', 'var7', 'var8', 'var9', 'var10', 'var11'],
        exit_code=2,
        stdout_regex='At most 10 positional arguments can be given',
    )

with test_env({'.qs.cfg': 'cmd ='}) as env:
    run_test(env, 'no value after action',
        ['cmd', '--dry-run'],
        exit_code=1,
        stderr_regex=r"Error in (.*)\.qs\.cfg: No value after '='",
    )

with test_env({'.qs.cfg': 'flags := --foo bar\ncmd=command ${flags}'}) as env:
    run_test(env, 'works with predefined variables',
             ['cmd', '--dry-run'],
             stdout_regex=r"Would run: (.*?) command --foo bar"
    )
    run_test(env, 'can override config flags',
             ['cmd', '--flags', 'overwritten', '--dry-run'],
             stdout_regex=r"Would run: (.*?) command overwritten"
    )

with test_env({'.qs.cfg': 'flags :='}) as env:
    run_test(env, 'errors on missing variable value',
             ['cmd'],
             exit_code=1,
             stderr_regex=r"Error in (.*)\.qs\.cfg: Expected '=' or ':='",
    )


with test_env({
    'source/root/.git/config': '',
    'source/root/.qs.cfg': 'cmd=cat file.txt',
    'source/root/file.txt': 'source-root/file.txt',
    'source/root/src/files/file.c': '',
    'a/b/ab.cfg': 'cmd=cat file.txt',
    'a/b/file.txt': 'root/a/b/file.txt',
    '.qs.cfg': 'cmd=cat file.txt',
    'file.txt': 'root/file.txt',
}) as env:
    run_test(env, 'runs from --config config dir', ['cmd', '--config', 'a/b/ab.cfg'],
        stdout='root/a/b/file.txt'
    )
    run_test(env, 'runs from pwd config dir', ['cmd'],
        stdout='root/file.txt'
    )
    run_test(env, 'runs from source root dir', ['cmd'],
        stdout='source-root/file.txt',
        run_from_dir='source/root/src/files',
    )

with test_env({
    'workdir/file': '',
    '.git/config': '',
    '.qs.cfg': 'cmd=echo "qs run from $$QS_RUN_DIR"',
}) as env:
    run_test(env, 'sets QS_RUN_DIR', ['cmd'],
        stdout='qs run from %s/workdir' % env,
        run_from_dir='workdir',
    )

with test_env({
    '.qs.cfg': 'build=sh build.sh',
    'build.sh': 'set -e\necho "Build OK"',
}) as env:
    run_test(env, 'running local scripts',
        ['build'],
        stdout='Build OK'
    )

# ===============================

num_passed = num_run - num_failed - num_skipped
if len(error_reports) > 0:
    print("\n\n----------------------- Failing tests -----------------------\n")
    for report in error_reports:
        print(report)
print(f"\nRan {num_run} tests: {CGREEN}{num_passed} passed{CEND}, {CYELLOW}{num_skipped}{CEND} skipped, {CRED}{num_failed} failed{CEND}")
