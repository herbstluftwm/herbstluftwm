def test_version(hlwm):
    proc = hlwm.call('version')
    assert proc.stdout.startswith('herbstluftwm')
