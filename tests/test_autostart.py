import textwrap
import time


def run_autostart(hlwm, tmpdir, autostart_src):
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

    # wait for the autostart to terminate:
    while hlwm.attr.autostart.running():
        time.sleep(1)


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
    run_autostart(hlwm,
                  tmpdir,
                  """
                  exit 4
                  """)
    assert hlwm.attr.autostart.last_status() == 4
