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
    proc.wait(20)
    assert proc.returncode == 0


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
    proc.wait(20)
    assert proc.returncode == 0
