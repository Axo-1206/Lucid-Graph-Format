#!/usr/bin/env python3
"""
@file tests/integration/run_lucid_tests.py

@brief Integration tests for the `lucid` binary.

─── What this tests ─────────────────────────────────────────────────────
The binary as a program: does it start, does it parse argv, does it exit
with the right code, does it write the file it says it will. The
libraries are covered by the unit tests; this covers the outer shell.

─── Usage ───────────────────────────────────────────────────────────────
    python3 run_lucid_tests.py <path-to-lucid> <fixture-dir>

Both arguments are required. The runner prints one line per scenario
and a final summary. Exit code is 0 if every scenario passed, 1
otherwise.

─── Design: no dependencies ─────────────────────────────────────────────
Standard library only. No pytest, no unittest — a plain script with a
list of functions is enough for the scale of this test suite, and it
avoids a test-framework dependency in the build.
"""

import os
import shutil
import subprocess
import sys
import tempfile


# ─── The scenario framework ───────────────────────────────────────────────

class Failure(Exception):
    """Raised by a check when the scenario does not hold."""
    pass


def check(condition, message):
    """Assert `condition`. Raise Failure with `message` if false."""
    if not condition:
        raise Failure(message)


def run(args, *, stdin=None, cwd=None):
    """
    Run a command, capture stdout and stderr, return the result.

    Returns a tuple (returncode, stdout_bytes, stderr_bytes).
    """
    proc = subprocess.run(
        args,
        input=stdin,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        cwd=cwd,
        check=False,
    )
    return proc.returncode, proc.stdout, proc.stderr


# ─── The scenarios ────────────────────────────────────────────────────────

def scenario_top_level_help(ctx):
    rc, out, err = run([ctx.lucid, "--help"])
    check(rc == 0, f"expected exit 0, got {rc}")
    text = out.decode("utf-8", errors="replace")
    check("Usage: lucid" in text, "help text missing 'Usage: lucid'")
    check("format" in text, "help text missing 'format'")
    check("check" in text, "help text missing 'check'")
    check("compile" in text, "help text missing 'compile'")


def scenario_top_level_version(ctx):
    rc, out, err = run([ctx.lucid, "--version"])
    check(rc == 0, f"expected exit 0, got {rc}")
    text = out.decode("utf-8", errors="replace").strip()
    check(text.startswith("lucid "), f"version line does not start with 'lucid ': {text!r}")


def scenario_no_args_shows_help(ctx):
    rc, out, err = run([ctx.lucid])
    check(rc == 0, f"expected exit 0, got {rc}")
    text = out.decode("utf-8", errors="replace")
    check("Usage: lucid" in text, "bare invocation did not print help")


def scenario_unknown_subcommand(ctx):
    rc, out, err = run([ctx.lucid, "frobnicate"])
    check(rc == 2, f"expected exit 2, got {rc}")
    text = err.decode("utf-8", errors="replace")
    check("unknown subcommand" in text, f"stderr missing 'unknown subcommand': {text!r}")


def scenario_format_help(ctx):
    rc, out, err = run([ctx.lucid, "format", "--help"])
    check(rc == 0, f"expected exit 0, got {rc}")
    text = out.decode("utf-8", errors="replace")
    check("lucid format" in text, "format help missing its usage line")


def scenario_check_help(ctx):
    rc, out, err = run([ctx.lucid, "check", "--help"])
    check(rc == 0, f"expected exit 0, got {rc}")
    text = out.decode("utf-8", errors="replace")
    check("lucid check" in text, "check help missing its usage line")


def scenario_compile_help(ctx):
    rc, out, err = run([ctx.lucid, "compile", "--help"])
    check(rc == 0, f"expected exit 0, got {rc}")
    text = out.decode("utf-8", errors="replace")
    check("lucid compile" in text, "compile help missing its usage line")


def scenario_format_valid_file(ctx):
    src = os.path.join(ctx.fixtures, "simple.lucid")
    rc, out, err = run([ctx.lucid, "format", src])
    check(rc == 0, f"expected exit 0, got {rc}; stderr: {err!r}")
    text = out.decode("utf-8", errors="replace")
    check("node speed" in text, f"formatted output missing 'node speed': {text!r}")


def scenario_check_valid_file(ctx):
    src = os.path.join(ctx.fixtures, "simple.lucid")
    rc, out, err = run([ctx.lucid, "check", src])
    check(rc == 0, f"expected exit 0, got {rc}; stderr: {err!r}")


def scenario_check_bad_file(ctx):
    src = os.path.join(ctx.fixtures, "bad_syntax.lucid")
    rc, out, err = run([ctx.lucid, "check", src])
    check(rc == 1, f"expected exit 1, got {rc}")
    text = err.decode("utf-8", errors="replace")
    check("ERROR" in text, f"stderr missing an ERROR line: {text!r}")


def scenario_compile_writes_default_output(ctx):
    # The binary writes `foo.lucgraph` next to `foo.lucid`. We copy
    # the fixture to a temp dir so we do not write into the fixture
    # tree.
    src = os.path.join(ctx.work, "simple.lucid")
    shutil.copy(os.path.join(ctx.fixtures, "simple.lucid"), src)

    rc, out, err = run([ctx.lucid, "compile", src])
    check(rc == 0, f"expected exit 0, got {rc}; stderr: {err!r}")

    out_path = os.path.join(ctx.work, "simple.lucgraph")
    check(os.path.isfile(out_path), f"output file not written: {out_path}")

    with open(out_path, "rb") as f:
        head = f.read(4)
    check(head == b"LUGR", f"output does not start with 'LUGR': {head!r}")


def scenario_compile_explicit_output(ctx):
    src = os.path.join(ctx.work, "simple.lucid")
    shutil.copy(os.path.join(ctx.fixtures, "simple.lucid"), src)

    out_path = os.path.join(ctx.work, "custom.lucgraph")
    rc, out, err = run([ctx.lucid, "compile", "-o", out_path, src])
    check(rc == 0, f"expected exit 0, got {rc}; stderr: {err!r}")
    check(os.path.isfile(out_path), f"output file not written: {out_path}")


def scenario_compile_stdin_requires_output(ctx):
    # No -o with stdin input is a usage error, exit 2.
    rc, out, err = run(
        [ctx.lucid, "compile", "-"],
        stdin=b"node x = Float32Node(1.0)\n",
    )
    check(rc == 2, f"expected exit 2, got {rc}")
    text = err.decode("utf-8", errors="replace")
    check("-o" in text, f"stderr missing '-o' requirement: {text!r}")


def scenario_compile_stdin_with_output(ctx):
    out_path = os.path.join(ctx.work, "stdin.lucgraph")
    rc, out, err = run(
        [ctx.lucid, "compile", "-o", out_path, "-"],
        stdin=b"node x = Float32Node(1.0)\n",
    )
    check(rc == 0, f"expected exit 0, got {rc}; stderr: {err!r}")
    check(os.path.isfile(out_path), f"output file not written: {out_path}")

    with open(out_path, "rb") as f:
        head = f.read(4)
    check(head == b"LUGR", f"output does not start with 'LUGR': {head!r}")


def scenario_check_missing_file(ctx):
    bogus = os.path.join(ctx.work, "does-not-exist.lucid")
    rc, out, err = run([ctx.lucid, "check", bogus])
    check(rc == 2, f"expected exit 2, got {rc}")
    text = err.decode("utf-8", errors="replace")
    check("cannot open file" in text, f"stderr missing 'cannot open file': {text!r}")


def scenario_check_rejects_o(ctx):
    src = os.path.join(ctx.fixtures, "simple.lucid")
    rc, out, err = run([ctx.lucid, "check", "-o", "/tmp/x", src])
    check(rc == 2, f"expected exit 2, got {rc}")
    text = err.decode("utf-8", errors="replace")
    check("does not accept" in text or "-o" in text,
          f"stderr missing -o rejection: {text!r}")


def scenario_version_after_subcommand_rejected(ctx):
    rc, out, err = run([ctx.lucid, "format", "--version"])
    check(rc == 2, f"expected exit 2, got {rc}")


# ─── The runner ───────────────────────────────────────────────────────────

SCENARIOS = [
    scenario_top_level_help,
    scenario_top_level_version,
    scenario_no_args_shows_help,
    scenario_unknown_subcommand,
    scenario_format_help,
    scenario_check_help,
    scenario_compile_help,
    scenario_format_valid_file,
    scenario_check_valid_file,
    scenario_check_bad_file,
    scenario_compile_writes_default_output,
    scenario_compile_explicit_output,
    scenario_compile_stdin_requires_output,
    scenario_compile_stdin_with_output,
    scenario_check_missing_file,
    scenario_check_rejects_o,
    scenario_version_after_subcommand_rejected,
]


class Context:
    def __init__(self, lucid, fixtures, work):
        self.lucid = lucid
        self.fixtures = fixtures
        self.work = work


def main():
    if len(sys.argv) != 3:
        print(f"usage: {sys.argv[0]} <path-to-lucid> <fixture-dir>",
              file=sys.stderr)
        return 2

    lucid = os.path.abspath(sys.argv[1])
    fixtures = os.path.abspath(sys.argv[2])

    if not os.path.isfile(lucid):
        print(f"error: lucid binary not found: {lucid}", file=sys.stderr)
        return 2

    work = tempfile.mkdtemp(prefix="lucid_integration_")
    ctx = Context(lucid, fixtures, work)

    passed = 0
    failed = 0
    failures = []

    try:
        for scenario in SCENARIOS:
            name = scenario.__name__.removeprefix("scenario_")
            try:
                scenario(ctx)
                print(f"PASS  {name}")
                passed += 1
            except Failure as e:
                print(f"FAIL  {name}: {e}")
                failures.append((name, str(e)))
                failed += 1
            except Exception as e:  # pylint: disable=broad-except
                print(f"ERROR {name}: unexpected exception: {e!r}")
                failures.append((name, repr(e)))
                failed += 1
    finally:
        shutil.rmtree(work, ignore_errors=True)

    print()
    print(f"{passed} passed, {failed} failed, {passed + failed} total")

    if failed:
        print()
        print("Failures:")
        for name, msg in failures:
            print(f"  {name}: {msg}")
        return 1

    return 0


if __name__ == "__main__":
    sys.exit(main())