def test_spawn(hlwm, hlwm_process):
    with hlwm_process.wait_stderr_match('spawnyboi'):
        cmd = ['spawn', 'sh', '-c', 'echo >&2 spawnyboi']
        proc = hlwm.unchecked_call(cmd, read_hlwm_output=False)
        assert proc.returncode == 0
        assert not proc.stderr
        assert not proc.stdout


def test_spawn_command_not_exist(hlwm, hlwm_process):
    cmdname = 'this_command_does_not_exist'
    with hlwm_process.wait_stderr_match(f'execvp "{cmdname}" failed'):
        cmd = ['spawn', cmdname]
        proc = hlwm.unchecked_call(cmd, read_hlwm_output=False)
        assert proc.returncode == 0
        assert not proc.stderr
        assert not proc.stdout
