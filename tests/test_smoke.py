def test_version(hlwm):
    proc = hlwm.call('version')
    assert proc.stdout.startswith('herbstluftwm')

def test_crash(hlwm):
    hlwm.call('crash')

def test_sleep(hlwm):
    hlwm.call('sleep-10')

def test_freeze(hlwm):
    hlwm.call('freeze')

def test_crash_on_client(hlwm, create_client):
    hlwm.callstr('attr clients.crash_on_client true')
    create_client()
