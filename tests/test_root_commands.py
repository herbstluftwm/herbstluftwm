
def test_attr_cmd(hlwm):
    assert hlwm.get_attr('monitors.focus.name') == ''
    hlwm.call('attr')
    hlwm.callstr('attr tags')
    hlwm.callstr('attr tags.')
    hlwm.callstr('attr tags.count')
    assert hlwm.call('attr', 'tags.count').stdout == hlwm.get_attr('tags.count')
    hlwm.fails('attr', 'tags.co')

def test_object_tree(hlwm):
    t1 = hlwm.call('object_tree').stdout.splitlines()
    t2 = hlwm.call('object_tree', 'theme.').stdout.splitlines()
    t3 = hlwm.call('object_tree', 'theme.tiling.').stdout.splitlines()
    assert len(t1) > len(t2)
    assert len(t2) > len(t3)

def test_sprintf(hlwm):
    cnt = hlwm.get_attr('tags.count')
    wmname = hlwm.get_attr('settings.wmname')
    printed = hlwm.callstr('sprintf X %s/%s tags.count settings.wmname echo X').stdout
    hlwm.fails('sprintf X %s/%s tags.count settings.wmname')
    hlwm.fails('sprintf X %s/%s tags.count')
    assert printed == cnt + '/' + wmname + '\n'
    assert '%\n' == hlwm.callstr('sprintf X %% echo X').stdout
