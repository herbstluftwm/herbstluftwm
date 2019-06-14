import pytest
import re

# example values for the respective types
ATTRIBUTE_TYPE_EXAMPLE_VALUES = \
    {
        'int': [23, 42, -8],
        'bool': ['true', 'false'],
        'string': ['foo', 'baz', 'bar'],
        'color': ['#ff00ff', '#9fbc00'],  # FIXME: include named colors
        'uint': [23, 42]
    }
ATTRIBUTE_TYPES = list(ATTRIBUTE_TYPE_EXAMPLE_VALUES.keys())


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


def test_set_attr_can_not_set_writable(hlwm):
    assert hlwm.call_xfail('set_attr tags.count 5') \
        .returncode == 3


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


def test_disjoin_rects(hlwm):
    # test the example from the manpage
    expected = '\n'.join((
        '300x150+300+250',
        '600x250+0+0',
        '300x150+0+250',
        '300x150+600+250',
        '600x250+300+400',
        ''))  # trailing newline
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


@pytest.mark.parametrize('attrtype', ATTRIBUTE_TYPES)
@pytest.mark.parametrize('name', ['my_test', 'my_foo'])
@pytest.mark.parametrize('object_path', ['', 'clients', 'theme.tiling.active'])
def test_new_attr_without_removal(hlwm, attrtype, name, object_path):
    path = (object_path + '.' + name).lstrip('.')

    hlwm.call(['new_attr', attrtype, path])

    hlwm.get_attr(path)


@pytest.mark.parametrize('attrtype', ATTRIBUTE_TYPES)
def test_new_attr_existing_builtin_attribute(hlwm, attrtype):
    hlwm.get_attr('monitors.count')
    hlwm.call_xfail(['new_attr', attrtype, 'monitors.count']) \
        .expect_stderr('attribute name must start with "my_"')


@pytest.mark.parametrize('attrtype', ATTRIBUTE_TYPES)
def test_new_attr_existing_user_attribute(hlwm, attrtype):
    path = 'theme.my_user_attr'
    hlwm.call(['new_attr', attrtype, path])
    hlwm.get_attr(path)

    hlwm.call_xfail(['new_attr', attrtype, path]) \
        .expect_stderr('already has an attribute')


@pytest.mark.parametrize('attrtype', ATTRIBUTE_TYPES)
@pytest.mark.parametrize('path', ['foo', 'monitors.bar'])
def test_new_attr_missing_prefix(hlwm, attrtype, path):
    hlwm.call_xfail(['new_attr', attrtype, path]) \
        .expect_stderr('must start with "my_"')


@pytest.mark.parametrize('attrtypevalues', ATTRIBUTE_TYPE_EXAMPLE_VALUES.items())
@pytest.mark.parametrize('path', ['my_foo', 'monitors.my_bar'])
def test_new_attr_is_writable(hlwm, attrtypevalues, path):
    (attrtype, values) = attrtypevalues
    hlwm.call(['new_attr', attrtype, path])
    for v in values:
        hlwm.call(['set_attr', path, v])
        assert hlwm.get_attr(path) == str(v)


@pytest.mark.parametrize('attrtype', ATTRIBUTE_TYPES)
def test_new_attr_has_right_type(hlwm, attrtype):
    path = 'my_user_attr'
    hlwm.call(['new_attr', attrtype, path])

    m = re.search('(.) . . ' + path, hlwm.call(['attr', '']).stdout)

    assert m.group(1)[0] == attrtype[0]


def test_remove_attr_invalid_attribute(hlwm):
    hlwm.call_xfail('remove_attr tags.invalid') \
        .expect_stderr('Object "tags" has no attribute "invalid".')


def test_remove_attr_invalid_child(hlwm):
    hlwm.call_xfail('remove_attr clients.foo.bar') \
        .expect_stderr('Object "clients." has no child named "foo"')


def test_remove_attr_non_user_path(hlwm):
    hlwm.call_xfail('remove_attr monitors.count') \
        .expect_stderr('Cannot remove built-in attribute "monitors.count"')


def test_remove_attr_user_attribute(hlwm):
    path = 'my_user_attr'
    hlwm.call(['new_attr', 'string', path])

    hlwm.call(['remove_attr', path])

    hlwm.call_xfail(['get_attr', path]).expect_stderr('has no attribute')  # attribute does not exist
    hlwm.call(['new_attr', 'string', path])  # and is free again


def test_getenv_completion(hlwm):
    prefix = 'some_uniq_prefix_'
    name = prefix + 'envname'
    hlwm.call(['setenv', name, 'myvalue'])

    assert [name] == hlwm.complete('getenv ' + prefix, position=1)


def test_compare_invalid_operator(hlwm):
    hlwm.call_xfail('compare monitors.count -= 1') \
        .expect_stderr('unknown operator')


@pytest.mark.parametrize('args', [[], ['abc'], ['foo', 'bar']])
def test_echo_command(hlwm, args):
    assert hlwm.call(['echo'] + args).stdout == ' '.join(args) + '\n'


def test_echo_completion(hlwm):
    # check that the exit code is right
    assert hlwm.complete('echo foo') == []
