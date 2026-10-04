def test_spawn(hlwm, hlwm_process):
    with hlwm_process.wait_stderr_match('spawnyboi'):
        cmd = ['spawn', 'sh', '-c', 'echo >&2 spawnyboi']
        proc = hlwm.unchecked_call(cmd, read_hlwm_output=False)
        assert proc.returncode == 0
        assert not proc.stderr
        assert not proc.stdout


def test_spawn_command_not_exist(hlwm, hlwm_process):
    cmdname = 'this_command_does_not_exist'
    hlwm.call_xfail(['spawn', cmdname]) \
        .expect_stderr(cmdname) \
        .expect_stderr('No such file')


def test_spawn_command_no_permission(hlwm, tmp_path, hlwm_process):
    dirname = str(tmp_path)
    hlwm.call_xfail(['spawn', dirname]) \
        .expect_stderr(dirname) \
        .expect_stderr('Permission denied')
