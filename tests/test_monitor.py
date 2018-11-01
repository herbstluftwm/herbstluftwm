
def test_add_remove_monitor(hlwm):
    assert hlwm.get_attr('monitors.count') == '1'
    assert hlwm.get_attr('monitors.focus.name') == ''
    tag_name = hlwm.get_attr('monitors.focus.tag')
    hlwm.call('add', 'bla')
    hlwm.call('add_monitor', '800x600+40+40', 'bla', 'new_mon')
    assert hlwm.get_attr('monitors.count') == '2'
    hlwm.call('remove_monitor', 0)
    assert hlwm.get_attr('monitors.count') == '1'
    assert hlwm.get_attr('monitors.focus.name') == 'new_mon'
    hlwm.call('add_monitor', '800x600+40+40', '', 'blub')
    assert hlwm.get_attr('monitors.1.tag') == tag_name

