import pytest


def test_attr_cmd(hlwm):
    assert hlwm.get_attr('monitors.focus.name') == ''
    hlwm.call('attr')
    hlwm.call('attr tags')
    hlwm.call('attr tags.')
    hlwm.call('attr tags.count')
    assert hlwm.call('attr tags.count').stdout == hlwm.get_attr('tags.count')
    hlwm.call_xfail('attr tags.co')

@pytest.mark.parametrize('object_path', ['', 'clients', 'theme', 'monitors'])
def test_object_completion(hlwm, object_path):
    assert hlwm.list_children(object_path) \
        == hlwm.list_children_via_attr(object_path)

def test_object_tree(hlwm):
    t1 = hlwm.call('object_tree').stdout.splitlines()
    t2 = hlwm.call('object_tree theme.').stdout.splitlines()
    t3 = hlwm.call('object_tree theme.tiling.').stdout.splitlines()
    assert len(t1) > len(t2)
    assert len(t2) > len(t3)


def test_substitute(hlwm):
    expected_output = hlwm.get_attr('tags.count') + '\n'

    call = hlwm.call('substitute X tags.count echo X')

    assert call.stdout == expected_output


@pytest.mark.parametrize('prefix', ['set_attr settings.',
                                    'attr settings.',
                                    'cycle_value ',
                                    'set '])
def test_set_attr_completion(hlwm, prefix):
    assert hlwm.complete(prefix + "swap_monitors_to_get_tag") \
        == 'false off on toggle true'.split(' ')


def test_set_attr_only_writable(hlwm):
    # attr completes read-only attributes
    assert hlwm.complete('attr monitors.c', position=1, partial=True) \
        == ['monitors.count ']
    # but set_attr does not
    assert hlwm.complete('set_attr monitors.c', position=1, partial=True) \
        == []


def test_attr_only_second_argument_if_writable(hlwm):
    # attr does not complete values for read-only attributes
    assert hlwm.call_xfail_no_output('complete 2 attr monitors.count') \
        .returncode == 7


def test_substitute_missing_attribute__command_treated_as_attribute(hlwm):
    call = hlwm.call_xfail('substitute X echo X')

    assert call.stderr == 'The root object has no attribute "echo"\n'


def test_substitute_command_missing(hlwm):
    call = hlwm.call_xfail('substitute X tags.count')

    assert call.stderr == 'substitute: not enough arguments\n'


def test_sprintf(hlwm):
    expected_count = hlwm.get_attr('tags.count')
    expected_wmname = hlwm.get_attr('settings.wmname')
    expected_output = expected_count + '/' + expected_wmname + '\n'

    call = hlwm.call('sprintf X %s/%s tags.count settings.wmname echo X')

    assert call.stdout == expected_output


def test_sprintf_too_few_attributes__command_treated_as_attribute(hlwm):
    call = hlwm.call_xfail('sprintf X %s/%s tags.count echo X')

    assert call.stderr == 'The root object has no attribute "echo"\n'


def test_sprintf_too_few_attributes_in_total(hlwm):
    call = hlwm.call_xfail('sprintf X %s/%s tags.count')

    assert call.stderr == 'sprintf: not enough arguments\n'


def test_sprintf_command_missing(hlwm):
    call = hlwm.call_xfail('sprintf X %s tags.count')

    assert call.stderr == 'sprintf: not enough arguments\n'


def test_sprintf_double_percentage_escapes(hlwm):
    call = hlwm.call('sprintf X %% echo X')

    assert call.stdout == '%\n'


def test_disjoint_rects(hlwm):
    # test the example from the manpage
    expected = '\n'.join((
        '300x150+300+250',
        '600x250+0+0',
        '300x150+0+250',
        '300x150+600+250',
        '600x250+300+400',
        '')) # trailing newline
    response = hlwm.call('disjoin_rects 600x400+0+0 600x400+300+250').stdout
    assert response == expected


def test_attribute_completion(hlwm):
    def complete(partialPath):
        return hlwm.complete('get_attr ' + partialPath,
                             partial=True, position=1)

    assert complete('monitors.') == ['monitors.0.',
                                     'monitors.by-name.',
                                     'monitors.count ',
                                     'monitors.focus.']
    assert complete('monitors.fo') == ['monitors.focus.']
    assert complete('monitors.count') == ['monitors.count ']
    assert complete('monitors.focus') == ['monitors.focus.']
    assert complete('monitors.fooob') == []
    assert complete('monitors.fooo.bar') == []
    assert len(complete('monitors.focus.')) >= 8
    assert complete('t') == ['tags.', 'theme.', 'tmp.']
    assert complete('') == [l + '.' for l in hlwm.list_children_via_attr('')]
