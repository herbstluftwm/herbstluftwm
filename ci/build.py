#!/usr/bin/env python3

import argparse
import os
import re
import subprocess as sp
import sys
from pathlib import Path


def tox(tox_args, build_dir):
    """
    Prepare environment for tox and execute it with the given arguments
    """
    tox_env = os.environ.copy()
    tox_env['PWD'] = build_dir
    sp.check_call(f'tox {tox_args}', shell=True, cwd=build_dir, env=tox_env)


parser = argparse.ArgumentParser()
parser.add_argument('--build-dir', type=str, required=True)
parser.add_argument('--build-type', type=str, choices=('Release', 'Debug'), required=True)
parser.add_argument('--compile', action='store_true')
parser.add_argument('--run-tests', action='store_true')
parser.add_argument('--build-docs', action='store_true')
parser.add_argument('--cxx', type=str)
parser.add_argument('--cc', type=str)
parser.add_argument('--check-using-std', action='store_true')
parser.add_argument('--iwyu', action='store_true')
parser.add_argument('--flake8', action='store_true')
parser.add_argument('--ccache', nargs='?', metavar='ccache dir', type=str,
                    const=os.environ.get('CCACHE_DIR') or True)
args = parser.parse_args()

repo = Path(__file__).resolve().parent.parent
build_dir = Path(args.build_dir)
build_dir.mkdir(exist_ok=True)

if args.check_using_std:
    sp.check_call(['./ci/check-using-std.sh'], cwd=repo)

if args.ccache:
    if args.ccache is not True:
        os.environ['CCACHE_DIR'] = args.ccache

    # Delete config to prevent carrying over state unintentionally
    conf = Path(os.environ.get('CCACHE_DIR') or (Path.home() / '.ccache')) / 'ccache.conf'
    if conf.exists():
        conf.unlink()

    # Set a reasonable size limit
    sp.check_call('ccache --max-size=500M', shell=True)

    # Wipe stats before build
    sp.check_call('ccache -z', shell=True)

    # Print config for confirmation:
    sp.check_call('ccache -p', shell=True)

build_env = os.environ.copy()
build_env.update({
    'CC': args.cc,
    'CXX': args.cxx,
    'CFLAGS': '--coverage -Werror -fsanitize=address,leak,undefined',
    'CXXFLAGS': '--coverage -Werror -fsanitize=address,leak,undefined',

    # Hash-verifying the compiler is required when building with
    # clang-and-tidy.sh (because the script's mtime is not stable) and for
    # other cases, the overhead is minimal):
    'CCACHE_COMPILERCHECK': 'content',

    # In case clang-and-tidy.sh is used for building, it will need this to call
    # clang-tidy:
    'CLANG_TIDY_BUILD_DIR': str(build_dir),
})

cmake_args = [
    '-GNinja',
    '-DCMAKE_EXPORT_COMPILE_COMMANDS=ON',
    f'-DCMAKE_BUILD_TYPE={args.build_type}',
    f'-DWITH_DOCUMENTATION={"YES" if args.build_docs else "NO"}',
    f'-DENABLE_CCACHE={"YES" if args.ccache else "NO"}',
]

sp.check_call(['cmake', *cmake_args, repo], cwd=build_dir, env=build_env)

if args.compile:
    sp.check_call(['bash', '-c', 'time ninja -v -k 10'], cwd=build_dir, env=build_env)

if args.ccache:
    sp.check_call(['ccache', '-s'])

if args.iwyu:
    # Check lexicographical order of #include directives (cheap pre-check)
    fix_include = sp.run('fix_include --dry_run --sort_only --reorder */*.{h,cpp,c}', cwd=repo, shell=True, executable='bash', stdout=sp.PIPE)
    print(re.sub(
        r">>> Fixing #includes in '[^']*'[\n\r]*No changes in file [^\n\r]*[\n\r]*",
        '',
        fix_include.stdout.decode()))
    if fix_include.returncode != 0:
        print('IWYU/fix_include made suggestions, please fix them')
        sys.exit(1)

    # Run include-what-you-use
    iwyu_out = sp.check_output(f'iwyu_tool -p . -j "$(nproc)" -- --transitive_includes_only --mapping_file={repo}/.hlwm.imp', shell=True, cwd=build_dir)

    # Check IWYU output, but ignore any suggestions to add things (those tend
    # to be overzealous):
    # the following regex checks that the : is followed by a
    # non-empty sequence of non-empty lines
    iwyu_out_str = iwyu_out.decode('ascii').replace('\r', '')
    reg = r'(\S+) should remove these lines:\n(([^\n]+\n)+)'
    complaints = set(re.findall(reg, iwyu_out_str))
    if complaints:
        print('IWYU made suggestions to remove things in the following files:')
        for filepath, changes, _ in complaints:
            print("===== {} should remove: =====\n{}\n".format(filepath, changes.rstrip()))
        print("After removing the above lines it might be necessary to add")
        print("additional forward declarations to make it build again.")
        print("")
        sys.exit(1)

if args.flake8:
    tox('-e flake8', build_dir)

if args.run_tests:
    # Suppress warnings about known memory leaks:
    os.environ['LSAN_OPTIONS'] = f"suppressions={repo}/ci/lsan-suppressions.txt"

    # First, run only the tests that are NOT marked to be excluded from code
    # coverage collection.
    tox('-e py38 -- -n auto --cache-clear -v -x -m "not exclude_from_coverage"', build_dir)

    # Create the code coverage report:
    sp.check_call('lcov --capture --directory . --output-file coverage.info', shell=True, cwd=build_dir)
    sp.check_call('lcov --remove coverage.info "/usr/*" --output-file coverage.info', shell=True, cwd=build_dir)
    sp.check_call('lcov --list coverage.info', shell=True, cwd=build_dir)
    (build_dir / 'coverage.info').rename(repo / 'coverage.info')

    # Run the tests that have been skipped before (without clearing the pytest cache this time):
    tox('-e py38 -- -n auto -v -x -m exclude_from_coverage', build_dir)
