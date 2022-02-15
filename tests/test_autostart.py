import textwrap
import time
import subprocess
import os
import pytest
import conftest
from conftest import BINDIR, HlwmProcess


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


def wait_actively_for(callback):
    """wait actively for the callback to return True
    """
    left_attempts = 20
    while left_attempts > 0:
        left_attempts -= 1
        if callback() is True:
            return
        time.sleep(1)

    assert False, "The returned value was not 'True'"


def run_autostart(hlwm, tmpdir, autostart_src, wait=True):
    tmpfile = tmpdir / 'custom_autostart'
    hc_path = hlwm.HC_PATH
    full_src = textwrap.dedent(f"""\
    #!/usr/bin/env bash

    hc() {{
        {hc_path} "$@"
    }}
    """)
    full_src += autostart_src

    tmpfile.ensure()
    tmpfile.write(full_src)
    tmpfile.chmod(0o755)
    hlwm.attr.autostart.path = tmpfile
    hlwm.call('reload')

    if wait:
        # wait for the autostart to terminate:
        wait_actively_for(lambda: not hlwm.attr.autostart.running())


def test_autostart_pid(hlwm, tmpdir):
    run_autostart(hlwm,
                  tmpdir,
                  """
                  hc new_attr int my_pid
                  # copy pid of bash to the attribute system
                  hc set_attr my_pid $$
                  """)
    assert hlwm.attr.autostart.last_status() == 0
    assert hlwm.attr.my_pid() == hlwm.attr.autostart.pid()


def test_autostart_running(hlwm, tmpdir):
    run_autostart(hlwm,
                  tmpdir,
                  """
                  hc new_attr bool my_running
                  # copy the value of 'running' during autostart execution
                  hc substitute VALUE autostart.running set_attr my_running VALUE
                  """)
    assert hlwm.attr.autostart.last_status() == 0
    assert hlwm.attr.my_running() is True
    assert hlwm.attr.autostart.running() is False


def test_autostart_last_status(hlwm, tmpdir):
    for status in [0, 1, 2, 4, 9]:
        run_autostart(hlwm,
                      tmpdir,
                      f"""
                      exit {status}
                      """)
        assert hlwm.attr.autostart.last_status() == status


def process_status(pid):
    ps_cmd = ['ps', '-p', str(pid), '-o', 'state']
    proc = subprocess.run(ps_cmd,
                          stdout=subprocess.PIPE,
                          universal_newlines=True)
    return proc.stdout.splitlines()[1].strip()


def test_autostart_sigstop(hlwm, tmpdir):
    run_autostart(hlwm,
                  tmpdir,
                  """
                  hc new_attr string my_attr firststop
                  kill -STOP $$
                  hc set_attr my_attr secondstop
                  kill -STOP $$
                  hc set_attr my_attr final
                  """,
                  wait=False)
    pid = hlwm.attr.autostart.pid()
    # wait until the process is 'stopped'
    wait_actively_for(lambda: process_status(pid) == 'T')
    # then, it is still marked as 'running' in hlwm:
    assert hlwm.attr.autostart.running() is True
    assert hlwm.attr.my_attr() == 'firststop'
    # resume it:
    subprocess.run(['kill', '-CONT', str(pid)])

    # wait until the process is 'stopped' again
    wait_actively_for(lambda: process_status(pid) == 'T')
    assert hlwm.attr.my_attr() == 'secondstop'

    # then, it is still marked as 'running' in hlwm:
    # by this we verify that the 'continuation' did not
    # trigger the wrong signal handler
    assert hlwm.attr.autostart.running() is True

    # finally, resume it a second time and let it terminate:
    subprocess.run(['kill', '-CONT', str(pid)])

    wait_actively_for(lambda: not hlwm.attr.autostart.running())

    assert hlwm.attr.my_attr() == 'final'
