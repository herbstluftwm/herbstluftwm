import re
import os
import pytest
import subprocess
import textwrap
from conftest import BINDIR, HlwmProcess
import conftest


HLWM_PATH = os.path.join(BINDIR, 'herbstluftwm')


def test_reload(hlwm_process, hlwm):
    with hlwm_process.wait_stdout_match('hlwm started'):
        # run the command, but read not hlwm's output in unchecked_call()
        # but instead, let the current context manager read it!
        proc = hlwm.unchecked_call('reload', read_hlwm_output=False)
        assert not proc.stderr
        assert not proc.stdout
        assert proc.returncode == 0


def test_herbstluftwm_already_running(hlwm):
    result = subprocess.run([HLWM_PATH],
                            stderr=subprocess.PIPE,
                            universal_newlines=True)
    assert result.returncode == 1
    assert re.search(r'another window manager is already running', result.stderr)


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
    hlwm_proc = HlwmProcess('', env, [])
    hlwm_proc.read_and_echo_output(until_stderr='Will not run autostart file.')
    hlwm_proc.shutdown()
