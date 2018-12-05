def test_default_monitor(hlwm):
    assert hlwm.get_attr('monitors.count') == '1'
    assert hlwm.get_attr('monitors.focus.name') == ''


def test_add_monitor_requires_unfocused_tag(hlwm):
    call = hlwm.call_xfail('add_monitor', '800x600+40+40', 'default', 'monitor2')

    assert hlwm.get_attr('monitors.count') == '1'
    assert hlwm.get_attr('monitors.focus.name') == ''


def test_add_monitor(hlwm):
    hlwm.call('add', 'tag2')

    hlwm.call('add_monitor', '800x600+40+40', 'tag2', 'monitor2')

    assert hlwm.get_attr('monitors.count') == '2'
    assert hlwm.get_attr('monitors.1.name') == 'monitor2'


def test_cannot_add_monitor_without_free_tag(hlwm):
    call = hlwm.call_xfail('add_monitor', '800x600+40+40')

    assert call.stderr.endswith(' not enough free tags\n')
    assert hlwm.get_attr('monitors.count') == '1'


def test_cannot_add_monitor_with_nonexistent_tag(hlwm):
    call = hlwm.call_xfail('add_monitor', '800x600+40+40', 'derp')

    assert call.stderr.endswith(' does not exist\n')
    assert hlwm.get_attr('monitors.count') == '1'


def test_cannot_add_monitor_with_already_viewed_tag(hlwm):
    hlwm.call('add', 'tag2')
    call = hlwm.call_xfail('add_monitor', '800x600+40+40', 'default')

    assert call.stderr.endswith(' already being viewed on a monitor\n')
    assert hlwm.get_attr('monitors.count') == '1'


def test_cannot_add_monitor_with_numeric_name(hlwm):
    hlwm.call('add', 'tag2')
    call = hlwm.call_xfail('add_monitor', '800x600+40+40', 'tag2', '123foo')

    assert call.stderr.endswith(' may not start with a number\n')
    assert hlwm.get_attr('monitors.count') == '1'


def test_cannot_add_monitor_with_empty_name(hlwm):
    hlwm.call('add', 'tag2')
    call = hlwm.call_xfail('add_monitor', '800x600+40+40', 'tag2', '')

    assert call.stderr.endswith(' empty monitor name is not permitted\n')
    assert hlwm.get_attr('monitors.count') == '1'


def test_cannot_add_monitor_with_existing_name(hlwm):
    hlwm.call('rename_monitor', '0', 'mon1')
    hlwm.call('add', 'tag2')
    call = hlwm.call_xfail('add_monitor', '800x600+40+40', 'tag2', 'mon1')

    assert call.stderr.endswith('" already exists\n')
    assert hlwm.get_attr('monitors.count') == '1'


def test_remove_monitor(hlwm):
    hlwm.call('add', 'tag2')
    hlwm.call('add_monitor', '800x600+40+40', 'tag2', 'monitor2')

    hlwm.call('remove_monitor', '0')

    assert hlwm.get_attr('monitors.count') == '1'
    assert hlwm.get_attr('monitors.focus.name') == 'monitor2'


def test_cannot_remove_nonexistent_monitor(hlwm):
    call = hlwm.call_xfail('remove_monitor', '1')

    assert call.stderr.endswith(' not found!\n')
    assert hlwm.get_attr('monitors.count') == '1'


def test_cannot_remove_last_monitor(hlwm):
    call = hlwm.call_xfail('remove_monitor', '0')

    assert call.stderr.endswith(' last monitor\n')
    assert hlwm.get_attr('monitors.count') == '1'
