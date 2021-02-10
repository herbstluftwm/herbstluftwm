import pytest
import os.path
import subprocess
import sys
import os
import conftest


def test_example(hlwm):
    # test the example.py shipped with the bindings
    example_py = os.path.join(os.path.dirname(__file__), '..', 'python', 'herbstluftwm', 'example.py')
    # make 'herbstclient' binary available in the PATH
    os.environ['PATH'] = conftest.BINDIR + ':' + os.environ['PATH']
    assert subprocess.call([sys.executable, example_py]) == 0


def test_attr_get(hlwm):
    assert hlwm.attr.tags.focus.index() == '0'
    assert str(hlwm.attr.tags.focus.index) == '0'


def test_multi_objects_format(hlwm):
    tag = hlwm.attr.tags.focus
    assert f'{tag.index} / {tag.frame_count}' == '0 / 1'


def test_attr_set(hlwm):
    hlwm.attr.tags.focus.name = 'newname'

    assert hlwm.call('get_attr tags.focus.name').stdout == 'newname'
    assert hlwm.attr.tags.focus.name() == 'newname'


def test_attr_custom_attribute(hlwm):
    hlwm.attr.monitors.my_new_attr = 'myvalue'

    assert hlwm.call('get_attr monitors.my_new_attr').stdout == 'myvalue'
    assert hlwm.attr.monitors.my_new_attr() == 'myvalue'


def test_attr_get_dict_child(hlwm):
    assert hlwm.attr.monitors['0'].index() == '0'
    assert hlwm.attr.monitors[0].index() == '0'


def test_attr_get_dict_attribute(hlwm):
    assert hlwm.attr.monitors['0']['index']() == '0'


def test_attr_set_dict_attribute(hlwm):
    hlwm.attr.monitors['0']['name'] = 'newname'

    assert hlwm.call('get_attr monitors.0.name').stdout == 'newname'


def test_attr_set_dict_custom_attribute(hlwm):
    hlwm.attr.monitors['0']['my_test'] = 'value'

    assert hlwm.call('get_attr monitors.0.my_test').stdout == 'value'


@pytest.mark.parametrize('value', ['a', 'b', 'c'])
def test_chain_commands_if_then_else(hlwm, value):
    from herbstluftwm import chain

    # perform the comparison value == 'a' in hlwm:
    hlwm.attr.my_attr = value
    cmd = chain('or', [
        chain('and', [
            ['compare', 'my_attr', '=', 'a'],
            chain('chain', [
                ['echo', 'then branch 1'],
                ['echo', 'then branch 2'],
            ]),
        ]),
        chain('chain', [
            ['echo', 'else branch 1'],
            ['echo', 'else branch 2'],
        ]),
    ])
    if value == 'a':
        expected = 'then branch 1\nthen branch 2\n'
    else:
        expected = 'else branch 1\nelse branch 2\n'

    output = hlwm.call(cmd).stdout

    assert output == expected
