import os
import os.path
import re
import shlex
import subprocess
import sys
import textwrap

import pytest


BINDIR = os.path.join(os.path.abspath(os.environ['PWD']))

class HlwmBridge:

    HC_PATH = os.path.join(BINDIR, 'herbstclient')

    def __init__(self, display, hlwm_process):
        self.client_procs = []
        self.next_client_id = 0;
        self.env = {
            'DISPLAY': display,
        }
        self.hlwm_process = hlwm_process
        self.hc_idle = subprocess.Popen(
                    [self.HC_PATH, '--idle', 'rule', 'here_is_.*'],
                    bufsize=1, # line buffered
                    universal_newlines=True,
                    env=self.env,
                    stdout=subprocess.PIPE)
        # a dictionary mapping wmclasses to window ids as reported
        # by self.hc_idle
        self.wmclass2winid = {}

    def _parse_command(self, cmd):
        """
        Parse a command (a string using shell quotes or
        a string list) to a string list.
        """
        if isinstance(cmd, list):
            args = [str(x) for x in cmd]
            assert args
        else:
            args = shlex.split(cmd)
        return args

    def unchecked_call(self, cmd, log_output=True):
        """call the command but do not check exit code or stderr"""
        args = self._parse_command(cmd)

        try:
            proc = subprocess.run([self.HC_PATH, '-n'] + args,
                                  stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                  env=self.env,
                                  universal_newlines=True,
                                  # Kill hc when it hangs due to crashed server:
                                  timeout=2
                                  )
        except subprocess.TimeoutExpired:
            self.hlwm_process.investigate_timeout('calling ' + str(args))

        outcome = 'succeeded' if proc.returncode == 0 else 'failed'
        allout = proc.stdout + proc.stderr
        if allout:
            if log_output:
                print(f'Client command {args} {outcome} with output:\n{allout}')
            else:
                print(f'Client command {args} {outcome} with output', end='')
                print(' (output suppressed).')
        else:
            print(f'Client command {args} {outcome} (no output)')
        return proc

    def call(self, cmd):
        """call the command and expect it to have exit code zero
        and no output on stderr"""
        proc = self.unchecked_call(cmd)
        assert proc.returncode == 0
        assert not proc.stderr
        return proc

    def call_xfail(self, cmd):
        proc = self.unchecked_call(cmd)
        assert proc.returncode != 0
        assert proc.stderr != ""
        return proc

    def call_xfail_no_output(self, cmd):
        proc = self.unchecked_call(cmd)
        assert proc.returncode != 0
        return proc

    def get_attr(self, attribute_path, check=True):
        return self.call(['get_attr', attribute_path]).stdout

    def create_client(self, title=None):
        """
        Launch a client that will be terminated on shutdown.
        """
        self.next_client_id += 1
        wmclass = 'client_{}'.format(self.next_client_id)
        title = ['-title', str(title)] if title else []
        command = ['xterm'] + title + ['-hold', '-class', wmclass, '-e', 'true']

        # enforce a hook when the window appears
        self.call(['rule', 'once', 'class='+wmclass, 'hook=here_is_'+wmclass])

        proc = subprocess.Popen(command, env=self.env)
        # once the window appears, the hook is fired:
        winid = self.wait_for_window_of(wmclass)

        self.client_procs.append(proc)
        return winid

    def complete(self, cmd, partial=False, position=None):
        """
        Return a sorted list of all completions for the next argument for the
        given command, if position=None. If position is given, then the
        argument of the given position is completed.

        Set 'partial' if some of the completions for the given command are
        partial. If not in 'partial' mode, trailing spaces are stripped.
        """
        args = self._parse_command(cmd)
        if position is None:
            position = len(args)
        proc = self.call(['complete_shell', position] + args)
        items = []
        for i in proc.stdout.splitlines(False):
            if partial:
                items.append(i)
            else:
                if not i.endswith(' '):
                    raise Exception(("completion for \"{}\" returned the partial "
                                    + "completion item \"{}\"").format(cmd, i)
                                    ) from None
                else:
                    items.append(i[0:-1])
        return sorted(items)

    def list_children_via_attr(self, object_path):
        """
        List the names of children of the
        given object, using the attr command internally.
        """
        # regexes for list_children:

        children_re = \
            re.compile(r'^[0-9]* (child|children)[\\.:]((\n  [^\n]*)*)')
        line_re = re.compile('^  (.*)\\.$')
        output = self.call(['attr', object_path]).stdout
        section_match = children_re.match(output)
        assert section_match
        children = []
        for i in section_match.group(2).split('\n')[1:]:
            child_match = line_re.match(i)
            assert child_match
            children.append(child_match.group(1))
        return sorted(children)

    def list_children(self, object_path):
        """
        List the names of children of the
        given object, using the complete_shell command.
        """
        if not object_path.endswith('.') and object_path != '':
            object_path += '.'
        items = self.complete(['object_tree', object_path],
                              partial=True, position=1)
        children = []
        for i in items:
            children.append(i.split('.')[-2])
        return sorted(children)

    def create_clients(self, num):
        return [self.create_client() for i in range(num)]

    def wait_for_window_of(self, wmclass):
        """Wait for a rule hook of the form "here_is_" + wmclass """
        # We don't need to read the second argument of the hook and don't need
        # to check that is indeed equals "here_is_" + wmclass. But we need to
        # check this once we create clients simultaneously.
        line = self.hc_idle.stdout.readline().rstrip('\n').split('\t')
        try:
            self.hc_idle.wait(0)
        except subprocess.TimeoutExpired:
            pass
        if self.hc_idle.returncode is not None:
            self.hlwm_process.investigate_timeout(
                'waiting for hook triggered by client \"{}\"'.format(wmclass))
        return line[-1]

    def shutdown(self):
        for client_proc in self.client_procs:
            client_proc.terminate()
            client_proc.wait(2)

        self.hc_idle.terminate()
        self.hc_idle.wait(2)

@pytest.fixture
def hlwm(hlwm_process):
    display = os.environ['DISPLAY']
    #display = ':13'
    hlwm_bridge = HlwmBridge(display, hlwm_process)
    yield hlwm_bridge
    hlwm_bridge.shutdown()


class HlwmProcess:
    def __init__(self, tmpdir, env):
        autostart = tmpdir / 'herbstluftwm' / 'autostart'
        autostart.ensure()
        autostart.write(textwrap.dedent("""
            #!/usr/bin/env bash
            echo "hlwm started"
        """.lstrip('\n')))
        autostart.chmod(0o755)
        bin_path = os.path.join(BINDIR, 'herbstluftwm')
        self.proc = subprocess.Popen([bin_path, '--verbose'], env=env,
                                stdout=subprocess.PIPE)
        line = self.proc.stdout.readline()
        assert line == b'hlwm started\n'

    def investigate_timeout(self, reason):
        """if some kind of client request observes a timeout, investigate the
        herbstluftwm server process. 'reason' is best phrased using present
        participle"""
        try:
            self.proc.wait(0)
        except subprocess.TimeoutExpired:
            pass
        if self.proc.returncode is None:
            raise Exception(str(reason) + " took too long"
                            + " but hlwm still running") from None
        else:
            raise Exception("{} made herbstluftwm quit with exit code {}"
                .format(str(reason), self.proc.returncode)) from None

    def shutdown(self):
        self.proc.terminate()
        if self.proc.returncode is None:
            # only wait the process if it hasn't been cleaned up
            # this also avoids the second exception if hlwm crashed
            try:
                assert self.proc.wait(2) == 0
            except subprocess.TimeoutExpired:
                self.proc.kill()
                self.proc.wait(2)
                raise Exception("herbstluftwm did not quit on sigterm"
                                + " and had to be killed") from None

@pytest.fixture(autouse=True)
def hlwm_process(tmpdir):
    env = {
        'DISPLAY': os.environ['DISPLAY'],
        'XDG_CONFIG_HOME': str(tmpdir),
    }
    #env['DISPLAY'] = ':13'
    hlwm_proc = HlwmProcess(tmpdir, env)

    yield hlwm_proc

    hlwm_proc.shutdown()


@pytest.fixture(params=[0])
def running_clients(hlwm, running_clients_num):
    """
    Fixture that provides a number of already running clients, as defined by a
    "running_clients_num" test parameter.
    """
    return hlwm.create_clients(running_clients_num)
