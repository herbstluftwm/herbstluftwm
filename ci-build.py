#!/usr/bin/env python3

import argparse
import os
import subprocess as sp
import tempfile
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('--build-type', type=str, choices=('Release', 'Debug'), required=True)
parser.add_argument('--run-tests', action='store_true')
parser.add_argument('--build-docs', action='store_true')
parser.add_argument('--cxx', type=str, required=True)
parser.add_argument('--cc', type=str, required=True)
parser.add_argument('--check-using-std', action='store_true')
parser.add_argument('--iwyu', action='store_true')
parser.add_argument('--ccache', nargs='?', metavar='ccache dir', type=str,
                    const=os.environ.get('CCACHE_DIR') or True)
args = parser.parse_args()

if args.check_using_std:
    sp.check_call(['./ci-check-using-std.sh'], cwd='/hlwm')

temp_dir = tempfile.TemporaryDirectory(dir='/hlwm', prefix='build.')
build_dir = temp_dir.name

if args.ccache:
    if args.ccache is not True:
        os.environ['CCACHE_DIR'] = args.ccache

    # Delete config to prevent carrying over state unintentionally
    conf = Path(os.environ.get('CCACHE_DIR') or (Path.home() / '.ccache')) / 'ccache.conf'
    if conf.exists():
        conf.unlink()

    # Ignore the working directory on cache lookups
    sp.check_call('ccache -o hash_dir=false', shell=True)

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
    })

cmake_args = [
    '-GNinja',
    '-DCMAKE_EXPORT_COMPILE_COMMANDS=ON',
    f'-DCMAKE_BUILD_TYPE={args.build_type}',
    f'-DWITH_DOCUMENTATION={"YES" if args.build_docs else "NO"}',
    f'-DENABLE_CCACHE={"YES" if args.ccache else "NO"}',
    ]

sp.check_call(['cmake', *cmake_args, '..'], cwd=build_dir, env=build_env)

sp.check_call(['bash', '-c', 'time ninja -v -k 10'], cwd=build_dir, env=build_env)

if args.ccache:
    sp.check_call(['ccache', '-s'])

if args.iwyu:
    # Check lexicographical order of #include directives (cheap pre-check)
    sp.check_call('fix_include --dry_run --sort_only --reorder /hlwm/src/*.h', shell=True)

    # Run include-what-you-use (just printing the result for now)
    sp.check_call('iwyu_tool -p . -j "$(nproc)"', shell=True, cwd=build_dir)

if args.run_tests:
    tox_env = os.environ.copy()
    tox_env.update({
        'PWD': build_dir,
        })
    sp.check_call(f'tox -e py37 -- -n auto -v -x', shell=True, cwd=build_dir, env=tox_env)
