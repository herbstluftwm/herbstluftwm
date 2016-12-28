def test_version(hlwm):
    proc = hlwm.send_command('version')
    assert proc.stdout.startswith('herbstluftwm')
