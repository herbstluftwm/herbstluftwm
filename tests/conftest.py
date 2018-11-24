import subprocess
import os.path
import os
import sys
import textwrap
from types import SimpleNamespace

import pytest


GIT_ROOT = os.path.join(os.path.abspath(os.path.dirname(__file__)), '..')


class HlwmBridge:

    HC_PATH = os.path.join(GIT_ROOT, 'herbstclient')

    def __init__(self,display):
        self.next_client_id = 0;
        self.env = {
            'DISPLAY': display,
        }

    def callstr(self, args, check=True):
        return self.call(*(args.split(' ')), check=check)
    def call(self, *args, check=True):
        assert args
        str_args = [ str(i) for i in args]
        proc = subprocess.run([self.HC_PATH, '-n'] + str_args,
                              stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                              env=self.env,
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

    def create_client(self):
        self.next_client_id += 1
        wmclass = 'client_{}'.format(self.next_client_id)
        command = ['xterm', '-hold', '-class', wmclass, '-e', 'true']
        # enforce a hook when the window appears
        self.call('rule', 'once', 'class='+wmclass, 'hook=here_is_'+wmclass)
        # run process that wait for the hook
        hc_wait = subprocess.Popen([self.HC_PATH, '--wait', 'rule', 'here_is_'+wmclass],
                    env=self.env,
                    stdout=subprocess.PIPE)
        # start the process...
        proc = subprocess.Popen(command,env=self.env)
        # once the window appears, the hook is fired, and the --wait exits:
        hc_wait.wait(2)
        winid = hc_wait.stdout.read().decode().rstrip('\n').split('\t')[-1]

        return SimpleNamespace(proc=proc, winid=winid)


@pytest.fixture
def hlwm():
    display = os.environ['DISPLAY']
    #display = ':13'
    return HlwmBridge(display)



@pytest.fixture(autouse=True)
def hlwm_process(tmpdir):
    env = {
        'DISPLAY': os.environ['DISPLAY'],
        'XDG_CONFIG_HOME': str(tmpdir),
    }
    #env['DISPLAY'] = ':13'
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
    #proc.wait()
    assert proc.wait(2) == 0


@pytest.fixture
def create_client(hlwm):
    """
    Callable fixture that allows to create clients that will be terminated on
    teardown.
    """
    clients = []

    def create_and_track_client():
        new_client = hlwm.create_client()
        clients.append(new_client)
        return new_client.winid

    yield create_and_track_client

    for client in clients:
        client.proc.terminate()
        client.proc.wait(2)
