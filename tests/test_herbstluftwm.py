import re
import os
import pytest
import subprocess
import textwrap
from conftest import BINDIR, HlwmProcess, HlwmBridge
import conftest
import os.path
from Xlib import X, Xatom


HLWM_PATH = os.path.join(BINDIR, 'herbstluftwm')


def test_reload(hlwm_process, hlwm):
    with hlwm_process.wait_stdout_match('hlwm started'):
        # run the command, but read not hlwm's output in unchecked_call()
        # but instead, let the current context manager read it!
        proc = hlwm.unchecked_call('reload', read_hlwm_output=False)
        assert not proc.stderr
        assert not proc.stdout
        assert proc.returncode == 0


@pytest.mark.parametrize("with_client", [True, False])
@pytest.mark.parametrize("explicit_arg", [True, False])
def test_wmexec_to_self(hlwm, hlwm_process, with_client, explicit_arg):
    if with_client:
        winid, _ = hlwm.create_client()
    # modify some attribute such that we can verify that hlwm re-booted
    hlwm.attr.settings.snap_gap = 13

    # Restart hlwm:
    args = [hlwm_process.bin_path, '--verbose'] if explicit_arg else []
    p = hlwm.unchecked_call(['wmexec'] + args,
                            read_hlwm_output=False)
    assert p.returncode == 0
    hlwm_process.read_and_echo_output(until_stdout='hlwm started')

    assert hlwm.attr.settings.snap_gap() != 13
    if with_client:
        assert winid in hlwm.list_children('clients')


@pytest.mark.parametrize("with_client", [True, False])
def test_wmexec_to_other(hlwm_process, xvfb, tmpdir, with_client):
    hlwm = HlwmBridge(xvfb.display, hlwm_process)
    if with_client:
        hlwm.create_client()

    file_path = tmpdir / 'witness.txt'
    assert not os.path.isfile(file_path)
    p = hlwm.unchecked_call(['wmexec', 'touch', file_path],
                            read_hlwm_output=False)
    assert p.returncode == 0

    # the hlwm process execs to 'touch' which then terminates on its own.
    hlwm_process.proc.wait()

    os.path.isfile(file_path)


def test_herbstluftwm_already_running(hlwm):
    result = subprocess.run([HLWM_PATH],
                            stderr=subprocess.PIPE,
                            universal_newlines=True)
    assert result.returncode == 1
    assert re.search(r'another window manager is already running', result.stderr)


def test_herbstluftwm_quit(hlwm_spawner, xvfb):
    hlwm_proc = hlwm_spawner(display=xvfb.display)
    hlwm = conftest.HlwmBridge(xvfb.display, hlwm_proc)

    assert hlwm.call('echo ping').stdout == 'ping\n'

    hlwm.call('quit')

    hlwm_proc.proc.wait(10)


def test_herbstluftwm_replace(hlwm_spawner, xvfb):
    hlwm_proc_old = hlwm_spawner(display=xvfb.display)
    hlwm_old = conftest.HlwmBridge(xvfb.display, hlwm_proc_old)
    assert hlwm_old.call('echo ping').stdout == 'ping\n'

    hlwm_proc_new = hlwm_spawner(display=xvfb.display, args=['--replace'])

    # --replace should make the old hlwm process shut down:
    hlwm_proc_old.proc.wait(10)

    # connect to new process
    hlwm_new = conftest.HlwmBridge(xvfb.display, hlwm_proc_new)
    assert hlwm_new.call('echo ping').stdout == 'ping\n'

    hlwm_proc_new.shutdown()


def test_herbstluftwm_default_autostart(hlwm):
    expected_tags = [str(tag) for tag in range(1, 10)]
    default_autostart = os.path.join(os.path.abspath(BINDIR), 'share/autostart')
    env_with_bindir_path = os.environ.copy()
    env_with_bindir_path['PATH'] = BINDIR + ":" + env_with_bindir_path['PATH']
    subprocess.run(['bash', '-e', default_autostart], check=True, env=env_with_bindir_path)

    assert hlwm.list_children('tags.by-name') == sorted(expected_tags)
    # Test a random setting different from the default in settings.h:
    assert hlwm.get_attr('settings.smart_frame_surroundings') == 'true'


@pytest.mark.parametrize("method", ['home', 'xdg', 'shortopt', 'longopt'])
def test_autostart_path(tmpdir, method, xvfb):
    # herbstluftwm environment:
    env = {
        'DISPLAY': xvfb.display,
    }
    args = []  # extra command line args
    if method == 'home':
        autostart = tmpdir / '.config' / 'herbstluftwm' / 'autostart'
        env['HOME'] = str(tmpdir)
    elif method == 'xdg':
        autostart = tmpdir / 'herbstluftwm' / 'autostart'
        env['XDG_CONFIG_HOME'] = str(tmpdir)
    elif method == 'longopt':
        autostart = tmpdir / 'somename'
        args += ['--autostart', str(autostart)]
    else:
        autostart = tmpdir / 'somename'
        args += ['-c', str(autostart)]

    autostart.ensure()
    autostart.write(textwrap.dedent("""
        #!/usr/bin/env bash
        echo "hlwm autostart test"
    """.lstrip('\n')))
    autostart.chmod(0o755)
    env = conftest.extend_env_with_whitelist(env)
    hlwm_proc = HlwmProcess('hlwm autostart test', env, args)

    # TODO: verify the path as soon as we have an autostart object

    hlwm_proc.shutdown()


def test_no_autostart(xvfb):
    # no HOME, no XDG_CONFIG_HOME
    env = {
        'DISPLAY': xvfb.display,
    }
    env = conftest.extend_env_with_whitelist(env)
    hlwm_proc = HlwmProcess('', env, [])
    hlwm_proc.read_and_echo_output(until_stderr='Will not run autostart file.')
    hlwm_proc.shutdown()


def test_herbstluftwm_help_flags():
    hlwm = os.path.join(BINDIR, 'herbstluftwm')
    for cmd in [[hlwm, '-h'], [hlwm, '--help']]:
        proc = subprocess.run(cmd,
                              stdout=subprocess.PIPE,
                              stderr=subprocess.PIPE,
                              universal_newlines=True)
        assert proc.returncode == 0
        assert proc.stderr == ''
        # look for some flag on stdout:
        assert re.search('--autostart', proc.stdout)


def test_herbstluftwm_unrecognized_option():
    hlwm = os.path.join(BINDIR, 'herbstluftwm')
    proc = subprocess.run([hlwm, '--foobar'],
                          stdout=subprocess.PIPE,
                          stderr=subprocess.PIPE,
                          universal_newlines=True)
    assert proc.returncode != 0
    assert re.search('unrecognized option \'--foobar\'', proc.stderr)


def test_herbstluftwm_version_flags():
    hlwm = os.path.join(BINDIR, 'herbstluftwm')
    for cmd in [[hlwm, '-v'], [hlwm, '--version']]:
        proc = subprocess.run(cmd,
                              stdout=subprocess.PIPE,
                              stderr=subprocess.PIPE,
                              universal_newlines=True)
        assert proc.returncode == 0
        assert proc.stderr == ''
        # look for some flag on stdout:
        assert re.search('herbstluftwm', proc.stdout)


def test_command_not_found(hlwm):
    command = 'nonexistentcommand'
    message = f'Command "{command}" not found'
    hlwm.call_xfail(command).expect_stderr(message)
    hlwm.call_xfail(f'{command} argument').expect_stderr(message)
    call = hlwm.unchecked_call(f'chain , echo foo , {command} argument , anothercmd')
    assert re.search(message, call.stderr)
    assert re.search('foo', call.stdout)


class IpcClient:
    WM_CLASS = 'HERBST_IPC_CLASS'
    IPC_ARGS = '_HERBST_IPC_ARGS'

    def __init__(self, x11):
        self.x11 = x11
        self.win = x11.root.create_window(
            0, 0, 50, 50,  # geometry
            2,  # border width
            x11.screen.root_depth,
            X.InputOutput,
            X.CopyFromParent)
        self.win.set_wm_class(IpcClient.WM_CLASS, IpcClient.WM_CLASS)


def test_ipc_server_wrong_property_format(hlwm, hlwm_process, x11):
    ipcclient = IpcClient(x11)
    winid_str = x11.winid_str(ipcclient.win)
    message = f"error.*'{IpcClient.IPC_ARGS}'.*{winid_str}.*"
    message += "expected format=8 but.*format=32"
    with hlwm_process.wait_stderr_match(re.compile(message)):
        ipcclient.win.change_property(
            x11.display.intern_atom(IpcClient.IPC_ARGS),
            Xatom.STRING, 32,  # property type and format
            [12, 24])
        x11.display.flush()


def test_ipc_server_wrong_property_type(hlwm, hlwm_process, x11):
    ipcclient = IpcClient(x11)
    winid_str = x11.winid_str(ipcclient.win)
    unknown_prop_type = "UNKNOWN_PROPERTY_TYPE"
    message = f"error.*'{IpcClient.IPC_ARGS}'.*{winid_str}.*{unknown_prop_type}"
    with hlwm_process.wait_stderr_match(re.compile(message)):
        ipcclient.win.change_property(
            x11.display.intern_atom(IpcClient.IPC_ARGS),
            x11.display.intern_atom(unknown_prop_type),
            8,  # property type and format
            [12, 24])
        x11.display.flush()
