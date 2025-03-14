import pytest
import subprocess
import sys
import os
import conftest
import pathlib
from herbstluftwm.types import Rectangle


def test_example(hlwm):
    # test the example.py shipped with the bindings
    example_py = pathlib.Path(__file__).parents[1] / 'python' / 'example.py'
    # make 'herbstclient' binary available in the PATH
    os.environ['PATH'] = str(conftest.BINDIR) + os.pathsep + os.environ['PATH']
    assert subprocess.call([sys.executable, example_py]) == 0


def test_attr_get(hlwm):
    assert hlwm.attr.tags.focus.index() == 0
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
    assert hlwm.attr.monitors['0'].index() == 0
    assert hlwm.attr.monitors[0].index() == 0


def test_attr_get_dict_attribute(hlwm):
    assert hlwm.attr.monitors['0']['index']() == 0


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


def test_implicit_type_conversion_bool(hlwm):
    hlwm.attr.my_bool = True  # implicitly creates an attribute
    assert hlwm.attr.my_bool() is True
    hlwm.attr.my_bool = False
    assert hlwm.attr.my_bool() is False
    hlwm.attr.my_bool = 'toggle'
    assert hlwm.attr.my_bool() is True


def test_implicit_type_conversion_int(hlwm):
    hlwm.attr.my_int = 32
    assert hlwm.attr.my_int() == 32

    hlwm.attr.my_int = '-=40'

    assert hlwm.attr.my_int() == -8


def test_implicit_type_conversion_uint(hlwm):
    hlwm.call('new_attr uint my_uint 32')
    assert hlwm.attr.my_uint() == 32

    hlwm.attr.my_uint = '-=40'

    assert hlwm.attr.my_uint() == 0


def test_implicit_type_conversion_string(hlwm):
    hlwm.attr.my_str = "test"
    assert hlwm.attr.my_str() == "test"

    hlwm.attr.my_str = "foo"
    assert hlwm.attr.my_str() == "foo"


def test_implicit_type_conversion_rectangle(hlwm):
    # TODO: change as soon as custom attributes support Rectangle!
    geo = Rectangle(10, 20, 400, 500)
    hlwm.attr.monitors.focus.geometry = geo
    assert hlwm.attr.monitors.focus.geometry() == geo

    geo = Rectangle(20, 30, 422, 522)
    hlwm.attr.monitors.focus.geometry = geo
    assert hlwm.attr.monitors.focus.geometry() == geo
