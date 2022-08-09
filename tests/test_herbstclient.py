import itertools
import subprocess
import os
import re
import pytest
import sys
import contextlib
from conftest import PROCESS_SHUTDOWN_TIME, HcIdle
from Xlib import X, Xatom

HC_PATH = os.path.join(os.path.abspath(os.environ['PWD']), 'herbstclient')


@pytest.mark.parametrize('argument', ['version', '--idle'])
def test_herbstclient_no_display(argument):
    result = subprocess.run([HC_PATH, argument],
                            stderr=subprocess.PIPE,
                            env={'DISPLAY': 'somethingwrong'},
                            universal_newlines=True)
    assert re.search(r'Cannot open display', result.stderr)
    assert result.returncode == 1


@pytest.mark.parametrize('hlwm_mode', ['never started', 'sigterm', 'sigkill'])
@pytest.mark.parametrize('hc_parameter', ['true', '--wait'])
@pytest.mark.parametrize('hc_quiet', [True, False])
def test_herbstclient_recognizes_hlwm_not_running(hlwm_spawner, x11, hlwm_mode, hc_parameter, hc_quiet):
    if hlwm_mode == 'never started':
        pass
    else:
        # start hlwm and kill it in order to leave X in some inconsistent state
        hlwm_proc = hlwm_spawner()
        # ping that herbstclient still works here
        hc = subprocess.run([HC_PATH, 'echo', 'ping'],
                            stdout=subprocess.PIPE,
                            universal_newlines=True,
                            check=True)
        assert hc.stdout == 'ping\n'
        if hlwm_mode == 'sigterm':
            hlwm_proc.proc.terminate()
            hlwm_proc.proc.wait(1)
            # if we terminate it gently, then the property is cleaned up
            assert x11.get_property('__HERBST_HOOK_WIN_ID') is None
        elif hlwm_mode == 'sigkill':
            hlwm_proc.proc.kill()
            hlwm_proc.proc.wait(1)
            # if we kill it forcefully, then the property is not cleaned up
            # so check that we really test hc_command with an inconsistent
            # state
            assert x11.get_property('__HERBST_HOOK_WIN_ID') is not None
        else:
            assert False, 'assert that we have no typos above'

    # run herbstclient while no hlwm is running
    if hc_quiet:
        hc_command = [HC_PATH, '--quiet', hc_parameter]
    else:
        hc_command = [HC_PATH, hc_parameter]
    result = subprocess.run(hc_command, stderr=subprocess.PIPE, universal_newlines=True)

    assert result.returncode != 0
    if hc_quiet:
        assert re.search(r'Error: herbstluftwm is not running', result.stderr) is None
    else:
        assert re.search(r'Error: herbstluftwm is not running', result.stderr)


def test_herbstclient_invalid_regex():
    cmd = [HC_PATH, '--idle', '(unmat']
    result = subprocess.run(cmd, stderr=subprocess.PIPE, universal_newlines=True)
    assert result.returncode == 1
    assert re.search(r'Cannot parse regex', result.stderr)


def test_herbstclient_help_on_stdout():
    cmd = [HC_PATH, '--help']
    result = subprocess.run(cmd, stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE,
                            universal_newlines=True)
    assert result.returncode == 0
    assert result.stderr == ''
    assert re.search(r'--no-newline', result.stdout)
    assert re.search(r'--wait', result.stdout)


@pytest.mark.parametrize('repeat', range(0, 2))  # use more repetitions to test the race-condtion
@pytest.mark.parametrize('num_hooks_sent', [6])  # see below comment on the race-condition
@pytest.mark.parametrize('num_hooks_recv', range(1, 4 + 1))
def test_herbstclient_wait(hlwm, num_hooks_sent, num_hooks_recv, repeat):
    cmd = [HC_PATH, '--wait', '--count', str(num_hooks_recv), 'matcher']
    proc = subprocess.Popen(cmd,
                            stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE,
                            bufsize=1,  # line-buffered
                            universal_newlines=True)
    # FIXME: there's a race condition here:
    # we don't know how fast hc connects to hlwm, so the first hooks
    # might be send too early and not be in the output of 'proc'. This is
    # why we send much more then we want to receive (--count) and we perform
    # two hc-calls and hope that this gives hc --wait enough time to boot up.
    hlwm.close_persistent_pipe()  # make hlwm calls a bit slower
    hlwm.call('true')
    hlwm.call('true')
    hlwm.call('true')

    for _ in range(0, num_hooks_sent):
        hlwm.call('emit_hook nonmatch nonarg')
        hlwm.call('emit_hook matcher somearg')

    # first read output entirely to avoid blocking on the side
    # of herbstclient
    assert proc.stderr.read() == ''
    assert proc.stdout.read().splitlines() == \
        num_hooks_recv * ['matcher\tsomearg']
    proc.wait(PROCESS_SHUTDOWN_TIME)
    assert proc.returncode == 0
    proc.stdout.close()
    proc.stderr.close()


@pytest.mark.parametrize('repeat', range(0, 3))  # use more repetitions to test the race-condtion
def test_lastarg_only(hlwm, repeat):
    cmd = [HC_PATH, '--wait', '--count', '5', '--last-arg']
    proc = subprocess.Popen(cmd,
                            stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE,
                            bufsize=1,  # line-buffered
                            universal_newlines=True)
    # FIXME: there's a race condition here:
    # we don't know how fast hc connects to hlwm, so
    # we send two dummy commands and hope that in the mean-time of two full
    # herbstclient round-trips, 'proc' establishes a connection to hlwm's hook
    # window. Then, we hope that the first 'emit_hook a' isn't too early yet.
    hlwm.close_persistent_pipe()  # make hlwm calls a bit slower
    hlwm.call('true')
    hlwm.call('true')
    hooks = [
        ['a'],
        ['b', 'c'],
        ['d', 'ef'],
        ['ghi', 'jklm', 'nop'],
        ['qsdlkf', 'foo', 'slkf', 'last'],
    ]
    expected_lines = []
    for h in hooks:
        hlwm.call(['emit_hook'] + h)
        expected_lines.append(h[-1])

    # first read output entirely to avoid blocking on the side
    # of herbstclient
    assert proc.stdout.read().splitlines() == expected_lines
    proc.wait(PROCESS_SHUTDOWN_TIME)
    assert proc.returncode == 0
    proc.stdout.close()
    proc.stderr.close()


@pytest.mark.parametrize('zero_separated', [True, False])
def test_zero_byte_separator(hlwm, zero_separated):
    hc_idle = HcIdle(hlwm, zero_separated)

    hooks = [
        ['hook_without_args'],
        ['hook_with', 'arg'],
        ['hook_with', 'two', 'args'],
    ]
    for h in hooks:
        hlwm.call(['emit_hook'] + h)

    for h in hooks:
        assert h == hc_idle.read_hook()


def test_version_without_hlwm_running():
    for cmd in [[HC_PATH, '-v'], [HC_PATH, '--version']]:
        proc = subprocess.run(cmd,
                              stdout=subprocess.PIPE,
                              universal_newlines=True)
        # test that the output ends with a newline
        assert re.match('^herbstclient.*\n$', proc.stdout)


def test_quiet_without_hlwm_running():
    for flag in ['-q', '--quiet']:
        cmd = [HC_PATH, flag, 'echo', 'test']
        proc = subprocess.run(cmd,
                              stdout=subprocess.PIPE,
                              stderr=subprocess.PIPE,
                              universal_newlines=True)
        assert proc.returncode != 0
        assert proc.stdout == ''
        assert proc.stderr == ''


def test_quiet_with_hlwm_running(hlwm):
    # even with --quiet, herbstclient must produce output
    for flag in ['-q', '--quiet']:
        cmd = [HC_PATH, flag, 'echo', 'test']
        proc = subprocess.run(cmd,
                              stdout=subprocess.PIPE,
                              stderr=subprocess.PIPE,
                              universal_newlines=True)
        assert proc.returncode == 0
        assert proc.stdout == 'test\n'
        assert proc.stderr == ''


def test_missing_command():
    for cmd in [[HC_PATH], [HC_PATH, '-q']]:
        proc = subprocess.run(cmd,
                              stdout=subprocess.PIPE,
                              stderr=subprocess.PIPE,
                              universal_newlines=True)
        assert proc.returncode != 0
        assert proc.stdout == ''
        assert re.search('missing', proc.stderr)
        assert re.search('Usage: .*COMMAND', proc.stderr)


def test_invalid_options():
    # in the following, -c is an option, but requires a parameter
    for cmd in [[HC_PATH, '-X'], [HC_PATH, '--invalid-longopt'], [HC_PATH, '-c']]:
        proc = subprocess.run(cmd,
                              stdout=subprocess.PIPE,
                              stderr=subprocess.PIPE,
                              universal_newlines=True)
        assert proc.returncode != 0
        assert proc.stdout == ''
        assert re.search('option', proc.stderr)


def test_ensure_newline(hlwm):
    # if the command does not print "\n", then it is added:
    proc1 = subprocess.run([HC_PATH, 'get_attr', 'tags.count'],
                           stdout=subprocess.PIPE,
                           universal_newlines=True)
    proc2 = subprocess.run([HC_PATH, '-n', 'get_attr', 'tags.count'],
                           stdout=subprocess.PIPE,
                           universal_newlines=True)
    assert not proc2.stdout.endswith("\n")
    assert proc1.stdout == proc2.stdout + "\n"

    # if the command prints "\n", then nothing is changed:
    proc1 = subprocess.run([HC_PATH, 'echo', 'test'],
                           stdout=subprocess.PIPE,
                           universal_newlines=True)
    proc2 = subprocess.run([HC_PATH, '-n', 'echo', 'test'],
                           stdout=subprocess.PIPE,
                           universal_newlines=True)
    assert proc1.stdout == "test\n"
    assert proc2.stdout == "test\n"


class IpcServer:
    """A simple re-implementation of the ipc server
    for testing error-branches in herbstclient
    """
    OUTPUT = '_HERBST_IPC_OUTPUT'
    ERROR = '_HERBST_IPC_ERROR'
    STATUS = '_HERBST_IPC_EXIT_STATUS'

    def __init__(self, x11, create_hook_window=True, has_error_channel=True):
        self.x11 = x11
        self.display = x11.display
        self.screen = self.display.screen()
        if create_hook_window:
            self.window = self.screen.root.create_window(
                50, 50, 300, 200, 2,
                self.screen.root_depth,
                X.InputOutput,
                X.CopyFromParent)
            hook_win_id = [self.window.id]
            if has_error_channel:
                self.window.change_property(
                    x11.display.intern_atom('_HERBST_IPC_HAS_ERROR'),
                    Xatom.CARDINAL, 32, [1])
        else:
            hook_win_id = []
        self.screen.root.change_property(x11.display.intern_atom('__HERBST_HOOK_WIN_ID'),
                                         Xatom.ATOM, 32, hook_win_id)

        # listen for XCreateWindow events
        self.screen.root.change_attributes(event_mask=X.SubstructureNotifyMask)
        self.display.sync()
        self.hc_requests = []  # list of running 'hc' callers

    def wait_for_hc(self, attempts=3):
        """wait for at least one herbstclient to connect"""
        self.display.sync()
        while attempts >= 0:
            self.display.next_event()
            windows = self.screen.root.query_tree().children
            self.hc_requests = []
            for w in windows:
                print("checking window: " + self.x11.winid_str(w))
                if w.get_wm_class() == ('HERBST_IPC_CLASS', 'HERBST_IPC_CLASS'):
                    self.hc_requests.append(w)
            if len(self.hc_requests) > 0:
                return

            attempts -= 1

        raise Exception("No herbstclient instances showed up")

    def reply_prop(self, prop_name, prop_type, format, value):
        atom = self.display.intern_atom(prop_name)
        for w in self.hc_requests:
            w.change_property(atom, prop_type, format, value)
        self.display.sync()

    def reply_text_prop(self, text, utf8, prop_name):
        """write the given text property to all present hc clients"""
        prop_type = Xatom.STRING
        if utf8:
            prop_type = self.display.intern_atom('UTF8_STRING')
        self.reply_prop(prop_name, prop_type, 8, bytes(text, encoding='utf8'))

    def reply_output(self, text, utf8=True):
        """write the given text to the error channel
        of all present hc clients"""
        self.reply_text_prop(text, utf8, IpcServer.OUTPUT)

    def reply_error(self, text, utf8=True):
        """write the given text to the error channel
        of all present hc clients"""
        self.reply_text_prop(text, utf8, IpcServer.ERROR)

    def reply_status(self, status):
        """write the given exit status to all present hc clients"""
        self.reply_prop(IpcServer.STATUS, Xatom.ATOM, 32, [status])


@contextlib.contextmanager
def hc_context(args=['echo', 'ping']):
    proc = subprocess.Popen([HC_PATH] + args,
                            stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE,
                            universal_newlines=True)
    yield proc

    class Reply:
        pass

    reply = Reply()
    reply.returncode = proc.wait(3)
    reply.stdout = proc.stdout.read()
    reply.stderr = proc.stderr.read()
    proc.reply = reply
    # to make a failed ci easier to debug, forward channels:
    args_str = ' '.join(args)
    print(f'"hc {args_str}" exited with status {reply.returncode} and output: {reply.stdout}')
    print(f'"hc {args_str}" has the error output: {reply.stderr}', file=sys.stderr)
    proc.stdout.close()
    proc.stderr.close()


@pytest.mark.parametrize('repeat', range(0, 10))  # number of repetitions to detect race-conditions
@pytest.mark.parametrize('utf8', [True, False])
def test_ipc_reply_with_error_channel(x11, repeat, utf8):
    server = IpcServer(x11)
    with hc_context() as hc:
        server.wait_for_hc()
        server.reply_status(23)
        server.reply_error('err', utf8=utf8)
        server.reply_output('out', utf8=utf8)

    assert hc.reply.stdout == 'out\n'
    assert hc.reply.stderr == 'err\n'
    assert hc.reply.returncode == 23


def test_ipc_reply_without_error_channel(x11):
    server = IpcServer(x11, has_error_channel=False)
    with hc_context() as hc:
        server.wait_for_hc()
        server.reply_status(12)
        server.reply_output('out')

    assert hc.reply.stdout == 'out\n'
    assert hc.reply.stderr == ''
    assert hc.returncode == 12


@pytest.mark.parametrize('order', itertools.permutations([0, 1, 2]))
@pytest.mark.parametrize('faulty_reply_index', [0, 1, 2])
def test_ipc_reply_wrong_format(x11, order, faulty_reply_index):
    """Test that herbstclient handles wrong atom formats correctly.
    We test each of the components of the reply and each permutation
    to ensure that a possible partial reply is always freed correctly
    and never printed.
    """
    server = IpcServer(x11)

    def reply_status():
        server.reply_status(12)

    def reply_error():
        server.reply_error('EEE')

    def reply_output():
        server.reply_output('OOO')

    name_and_reply = [
        # tuples of functions. the first entry is a correct
        (IpcServer.STATUS, reply_status),
        (IpcServer.ERROR, reply_error),
        (IpcServer.OUTPUT, reply_output),
    ]

    with hc_context() as hc:
        server.wait_for_hc()
        for reply_idx in order:
            name, reply = name_and_reply[reply_idx]
            if reply_idx == faulty_reply_index:
                if name == IpcServer.STATUS:
                    # write a string to the 'status' atom
                    server.reply_prop(name, Xatom.STRING, 8, bytes('16', encoding='utf8'))
                else:
                    # write a cardinal to the error/output channel:
                    server.reply_prop(name, Xatom.CARDINAL, 32, [18])
            else:
                # send correct reply:
                reply()

    faulty_name, _ = name_and_reply[faulty_reply_index]
    assert hc.reply.stdout == ''
    assert re.search(f'could not get window property "{faulty_name}"', hc.reply.stderr)
    assert not re.search('EEE', hc.reply.stderr)
    assert hc.reply.returncode == 1


def test_command_tokenization_in_x11_property(hlwm):
    """
    Test that the string list is correctly encoded to and decoded
    from the x11 property. In particular, we test that '' as the last
    token is not dropped when reading the x11 property.
    """
    cmd2output = [
        (['echo', 'foo', '', 'bar'], 'foo  bar\n'),
        (['echo', '', 'foo'], ' foo\n'),
        (['echo', '', 'g'], ' g\n'),
        (['echo', 'x'], 'x\n'),
        (['echo', ''], '\n'),
        (['echo', 'foo', ''], 'foo \n'),
        (['echo', 'foo', '', '', 'bar'], 'foo   bar\n'),
        (['echo', 'foo', '', ''], 'foo  \n'),
    ]
    for cmd, output in cmd2output:
        assert hlwm.call(cmd).stdout == output
