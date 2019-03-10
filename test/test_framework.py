import subprocess
from contextlib import contextmanager
import os
import re
import tempfile
import shutil
import errno
import inspect

CRED = '\033[91m'
CGREEN = '\033[92m'
CREDBG = '\x1b[6;39;41m'
CGREENBG = '\x1b[6;30;42m'
CBLUE = '\033[94m'
CYELLOW = '\033[93m'
CEND = '\033[0m'

source_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
def build():
    global _binary_path
    print(subprocess.check_call(['make debug'], shell=True, cwd=source_root))
    print(f"\n{CYELLOW}Rebuilding: OK!{CEND}")
    _binary_path = os.path.join(source_root, 'bin', 'qs')

_test_name = None # Name of the test that's currently running
_test_env_root = None # the root directory where the test will run

_binary_path = None # The path to the tested qs binary
_registered_tests = [] # All registered tests to run
_verifiers = []        # List of verifiers created for each run
_error_reports = []    # Reports of all errors

class RunVerifier:
    def __init__(self, run_name, run_result, exit_code, stdout, stderr):
        self.run_name = run_name
        self.exit_code = exit_code
        self.stdout = stdout
        self.stderr = stderr
        self.run_result = run_result
        self.did_pass = None

    def and_expect(self, exit_code=0, stdout=None, stdout_regex=None, stderr=None, stderr_regex=None):
        global _error_reports, _tests_run
        def std_stream_err(stream_name, expected, actual):
            return f"\u2717 Unexpected {CRED}{stream_name}{CEND}:\nGot:\n[{CEND}{actual}]{CEND}\nExpected:\n[{CEND}{expected}]{CEND}"


        errors = []
        if exit_code != self.exit_code:
            errors.append(f"\u2717 Unexpected {CRED}exit code{CEND}: Expected: {exit_code}, got: {self.exit_code}")
        if stdout_regex is not None and not re.match(stdout_regex, self.stdout, re.MULTILINE | re.DOTALL):
            errors.append(std_stream_err('stdout (regex)', stdout_regex, self.stdout))
        if stdout is not None and stdout != self.stdout:
            errors.append(std_stream_err('stdout', stdout, self.stdout))
        if stderr is not None and stderr != self.stderr:
            errors.append(std_stream_err('stderr', stderr, self.stderr))
        if stderr_regex is not None and not re.match(stderr_regex, self.stderr, re.MULTILINE | re.DOTALL):
            errors.append(std_stream_err('stderr (regex)', stderr_regex, self.stderr))
        if len(errors) > 0:
            stdout_display = "\n".join(["|  %s" % line for line in self.stdout.split("\n")]) if self.stdout else "  N/A"
            stderr_display = "\n".join(["|  %s" % line for line in self.stderr.split("\n")]) if self.stderr else "  N/A"
            error_report = [
                f"{CRED}----------------------------------------------------------------{CEND}",
                f"{CREDBG}\u2717 Failed {CEND}{CRED} [{self.run_name}]{CEND}",
                "Execution information:",
                f"stdout:\n{stdout_display}{CEND}\nstderr:\n{stderr_display}{CEND}",
                "",
                f"{CRED}Failure(s):{CEND}",
                '\n'.join(errors),
                f"{CRED}----------------------------------------------------------------{CEND}",
                "\n\n"
            ]
            print("\n".join(error_report))
            self.did_pass = False
        else:
            self.did_pass = True
            print(f"{CGREENBG}\u2713 Passed {CEND} {CGREEN}[{self.run_name}] {CEND}")


def _register_test(test_fn, test_fs=None):
    global _registered_tests
    def run_test():
        global _test_name, _test_env_root
        _test_env_root = None
        _test_name = test_fn.__name__

        def run_with_root(root):
            _test_env_root = root
            test_fn_arity = len(inspect.getargspec(test_fn)[0])
            result = test_fn(root) if test_fn_arity > 0 else test_fn()

        if test_fs:
            tmp_root = tempfile.mkdtemp(prefix="qs-test-")
            _test_env_root = tmp_root
            try:
                for rel_path in test_fs.keys():
                    abs_path = os.path.abspath(os.path.join(tmp_root, rel_path))
                    assert abs_path.startswith(tmp_root)
                    rel_dir = os.path.dirname(abs_path)
                    subprocess.check_call(['mkdir', '-p', rel_dir])
                    with open(abs_path, 'w') as f:
                        f.write(test_fs[rel_path])
                run_with_root(tmp_root)
            finally:
                shutil.rmtree(tmp_root)
        else:
            run_with_root(source_root)
    _registered_tests.append(run_test)

def run(*qs_args, run_from_dir=None, env=None):
    global _test_name, _verifiers, _test_env_root, _binary_path
    if _test_env_root is not None:
        shutil.copy(_binary_path, os.path.join(_test_env_root))
        cwd = os.path.join(_test_env_root, run_from_dir) if run_from_dir else _test_env_root
        binary = os.path.join(_test_env_root, 'qs')
    else:
        cwd = source_root
        binary = _binary_path
    run_result = subprocess.run([binary, *qs_args], stdout=subprocess.PIPE, stderr=subprocess.PIPE, cwd=cwd, env=env)
    [actual_exit, actual_stdout, actual_stderr] = (run_result.returncode, run_result.stdout.decode('utf-8'), run_result.stderr.decode('utf-8'))
    actual_stdout = actual_stdout.rstrip()
    actual_stderr = actual_stderr.rstrip()
    run_name = "%s: qs %s" % (_test_name, ' '.join(qs_args))
    verifier = RunVerifier(run_name, run_result, exit_code=actual_exit, stdout=actual_stdout, stderr=actual_stderr)
    _verifiers.append(verifier) # Keep track of verifiers so we can tell if they never ran
    return verifier

def test(arg):
    if hasattr(arg, '__call__'):
        return _register_test(arg)
    else:
        return lambda test_fn: _register_test(test_fn, test_fs=arg)

def run_tests_and_report():
    global _registered_tests, _error_reports, _binary_path, _verifiers
    build()

    for test_fn in _registered_tests:
        test_fn()

    num_runs = len(_verifiers)
    num_passed = 0
    for verifier in _verifiers:
        if verifier.did_pass is None:
            raise RuntimeError("%s never ran!" % verifier.run_name)
        if verifier.did_pass:
            num_passed += 1
    num_failed = num_runs - num_passed

    if len(_error_reports) > 0:
        print("\n\n----------------------- Failing tests -----------------------\n")
        for report in _error_reports:
            print(report)

    failed_report = ', 0 failed' if num_failed == 0 else f", {CRED}{num_failed} failed{CEND}"
    print(f"\nRan {num_runs} test runs: {CGREEN}{num_passed} passed{CEND}{failed_report}")
# =======================================
