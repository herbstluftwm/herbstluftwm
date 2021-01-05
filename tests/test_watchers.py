

def test_watchers_count(hlwm):
    assert hlwm.attr.watchers.count() == '0'
    hlwm.call('watch clients.focus.title')
    assert hlwm.attr.watchers.count() == '1'
    hlwm.call('watch clients.focus.title')
    assert hlwm.attr.watchers.count() == '1'


def test_watchers_basic_change1(hlwm, hc_idle):
    attr = 'tags.focus.frame_count'
    hlwm.call(['watch', attr])

    hlwm.call('split explode')

    assert hc_idle.hooks() == [['attribute_changed', attr, '1', '2']]


def test_watchers_basic_change2(hlwm, hc_idle):
    attr = 'tags.focus.tiling.focused_frame.algorithm'
    hlwm.call('set_layout max')

    hlwm.call(['watch', attr])
    hlwm.call('set_layout grid')

    assert hc_idle.hooks() == [['attribute_changed', attr, 'max', 'grid']]


def test_watchers_not_yet_existing_attribute(hlwm, hc_idle):
    hlwm.call('watch tags.1.name')

    hlwm.call('add foo')

    expected_hook = ['attribute_changed', 'tags.1.name', '', 'foo']
    assert expected_hook in hc_idle.hooks()


def test_watchers_attribute_disappears(hlwm, hc_idle):
    hlwm.call('watch tags.by-name.default.name')

    hlwm.call('rename default othername')

    expected_hook = ['attribute_changed', 'tags.by-name.default.name', 'default', '']
    assert expected_hook in hc_idle.hooks()


def test_watchers_custom_attribute_create(hlwm, hc_idle):
    hlwm.call('watch tags.my_var')

    hlwm.call('new_attr int tags.my_var 40')

    expected_hook = ['attribute_changed', 'tags.my_var', '', '40']
    assert hc_idle.hooks() == [expected_hook]


def test_watchers_custom_attribute_remove(hlwm, hc_idle):
    hlwm.call('new_attr int monitors.my_var -37')
    hlwm.call('watch monitors.my_var')

    hlwm.call('remove_attr monitors.my_var')

    expected_hook = ['attribute_changed', 'monitors.my_var', '-37', '']
    assert hc_idle.hooks() == [expected_hook]
