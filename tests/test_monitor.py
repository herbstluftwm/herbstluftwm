def test_default_monitor(hlwm):
    assert hlwm.get_attr('monitors.count') == '1'
    assert hlwm.get_attr('monitors.focus.name') == ''


def test_add_monitor_requires_unfocused_tag(hlwm):
    call = hlwm.call('add_monitor', '800x600+40+40', 'default', 'monitor2', check=False)

    assert call.returncode != 0
    assert hlwm.get_attr('monitors.count') == '1'
    assert hlwm.get_attr('monitors.focus.name') == ''


def test_add_monitor(hlwm):
    hlwm.call('add', 'tag2')

    hlwm.call('add_monitor', '800x600+40+40', 'tag2', 'monitor2')

    assert hlwm.get_attr('monitors.count') == '2'
    assert hlwm.get_attr('monitors.1.name') == 'monitor2'


def test_remove_monitor(hlwm):
    hlwm.call('add', 'tag2')
    hlwm.call('add_monitor', '800x600+40+40', 'tag2', 'monitor2')

    hlwm.call('remove_monitor', '0')

    assert hlwm.get_attr('monitors.count') == '1'
    assert hlwm.get_attr('monitors.focus.name') == 'monitor2'
