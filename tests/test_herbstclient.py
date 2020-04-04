import subprocess
import os
import re
import pytest

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
def test_herbstclient_recognizes_hlwm_not_running(hlwm_spawner, x11, hlwm_mode, hc_parameter):
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
    hc_command = [HC_PATH, hc_parameter]
    result = subprocess.run(hc_command, stderr=subprocess.PIPE, universal_newlines=True)

    assert result.returncode != 0
    assert re.search(r'Error: herbstluftwm is not running', result.stderr)
