import subprocess
import os.path
import os
import sys
import textwrap

import pytest


GIT_ROOT = os.path.join(os.path.abspath(os.path.dirname(__file__)), '..')


class HlwmBridge:

    HC_PATH = os.path.join(GIT_ROOT, 'herbstclient')

    def callstr(self, args, check=True):
        return self.call(*(args.split(' ')), check=check)
    def call(self, *args, check=True):
        assert args
        str_args = [ str(i) for i in args]
        proc = subprocess.run([self.HC_PATH, '-n'] + str_args, check=check,
                              stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                              universal_newlines=True)
        print(list(args))
        print(proc.stdout)
        print(proc.stderr, file=sys.stderr)
        if check:
            assert proc.returncode == 0
            assert not proc.stderr
        return proc
    def fails(self, *args):
        assert self.call(*args, check=False).returncode != 0

    def get_attr(self, attribute_path, check=True):
        return self.call('get_attr', attribute_path).stdout

@pytest.fixture
def hlwm():
    return HlwmBridge()



@pytest.fixture(autouse=True)
def hlwm_process(tmpdir):
    env = {
        'DISPLAY': os.environ['DISPLAY'],
        'XDG_CONFIG_HOME': str(tmpdir),
    }
    autostart = tmpdir / 'herbstluftwm' / 'autostart'
    autostart.ensure()
    autostart.write(textwrap.dedent("""
        #!/usr/bin/env bash
        echo "hlwm started"
    """.lstrip('\n')))
    autostart.chmod(0o755)
    bin_path = os.path.join(GIT_ROOT, 'herbstluftwm')

    proc = subprocess.Popen([bin_path, '--verbose'], env=env,
                            stdout=subprocess.PIPE)
    line = proc.stdout.readline()
    assert line == b'hlwm started\n'

    yield
    proc.terminate()
    assert proc.wait(2) == 0
