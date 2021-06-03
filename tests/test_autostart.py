import textwrap
import time
import subprocess


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
    ps_cmd = ['ps', '-q', str(pid), '-o', 'state', '--no-headers']
    proc = subprocess.Popen(ps_cmd,
                            stdout=subprocess.PIPE,
                            universal_newlines=True)
    return proc.communicate()[0].strip()


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
