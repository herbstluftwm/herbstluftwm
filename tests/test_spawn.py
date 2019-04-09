def test_spawn(hlwm, hlwm_process):
    hlwm.call(['spawn', 'sh', '-c', 'echo >&2 spawnyboi'])

    hlwm_process.wait_stderr_match('spawnyboi')
