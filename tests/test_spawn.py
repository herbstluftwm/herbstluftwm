def test_spawn(hlwm, hlwm_process):
    with hlwm_process.wait_stderr_match('spawnyboi'):
        cmd = ['spawn', 'sh', '-c', 'echo >&2 spawnyboi']
        proc = hlwm.unchecked_call(cmd, read_hlwm_output=False)
        assert proc.returncode == 0
        assert not proc.stderr
        assert not proc.stdout
