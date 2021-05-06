import pytest


def test_color_rgb(hlwm):
    hlwm.attr.theme.color = '#9fbc00'
    assert hlwm.attr.theme.color() == '#9fbc00'


def test_color_rgba(hlwm):
    hlwm.attr.theme.color = '#9fbc0043'
    assert hlwm.attr.theme.color() == '#9fbc0043'

    hlwm.attr.theme.color = '#9fbc12ab'
    assert hlwm.attr.theme.color() == '#9fbc12ab'
    hlwm.call('compare theme.color = #9fbc12ab')
    hlwm.call('compare theme.color != #9fbc12ba')
    hlwm.call('compare theme.color != #9fbc13ab')

    hlwm.attr.theme.color = '#9fbc3300'
    assert hlwm.attr.theme.color() == '#9fbc3300'
    hlwm.call('compare theme.color = #9fbc3300')
    hlwm.call('compare theme.color != #9fbc33')

    hlwm.attr.theme.color = '#9fbc00ff'
    # no transparency is simplified to a normal rgb value:
    assert hlwm.attr.theme.color() == '#9fbc00'
    hlwm.call('compare theme.color = #9fbc00')
    hlwm.call('compare theme.color = #9fbc00ff')
    hlwm.call('compare theme.color != #9fbc00f0')

    # test alpha value '09' which should not be
    # misinterpreted as an invalid octal number:
    hlwm.attr.theme.color = '#9fbc1109'
    assert hlwm.attr.theme.color() == '#9fbc1109'
    hlwm.call('compare theme.color = #9fbc1109')


def test_color_invalid(hlwm):
    hlwm.call_xfail('set_attr theme.color #9xbc00') \
        .expect_stderr('cannot allocate color')

    hlwm.call_xfail('set_attr theme.color lilablassblau') \
        .expect_stderr('cannot allocate color')

    hlwm.call_xfail('set_attr theme.color #9fbc000') \
        .expect_stderr('cannot allocate color')

    hlwm.call_xfail('set_attr theme.color #9fbc00x') \
        .expect_stderr('cannot allocate color')

    hlwm.call_xfail('set_attr theme.color #9fbc00fg') \
        .expect_stderr('invalid alpha')

    hlwm.call_xfail('set_attr theme.color #9fbc00-3') \
        .expect_stderr('invalid alpha')


def test_color_names(hlwm):
    colors = [('red', '#ff0000'), ('green', '#00ff00'), ('blue', '#0000ff')]
    for name, rgb in colors:
        hlwm.attr.theme.color = name
        assert hlwm.attr.theme.color() == rgb


def test_int_uint_unparsable_suffix(hlwm):
    for attrtype in ['int', 'uint']:
        attr = 'my_' + attrtype
        hlwm.call(['new_attr', attrtype, attr])

        hlwm.call_xfail(['set_attr', attr, '3f']) \
            .expect_stderr('unparsable suffix: f')

        hlwm.call_xfail(['set_attr', attr, '3+']) \
            .expect_stderr('unparsable suffix')

        hlwm.call_xfail(['set_attr', attr, '3.0']) \
            .expect_stderr('unparsable suffix')

        hlwm.call_xfail(['set_attr', attr, '+3x']) \
            .expect_stderr('unparsable suffix')


def test_type_children_names(hlwm):
    types = hlwm.list_children('types')
    for t in types:
        assert hlwm.attr.types[t].fullname() == t


@pytest.mark.parametrize("int_or_uint", ['int', 'uint'])
def test_int_uint_relative_but_nonnegative(hlwm, int_or_uint):
    hlwm.call(['new_attr', int_or_uint, 'my_val'])

    hlwm.attr.my_val = 5
    hlwm.attr.my_val = '+=3'
    assert hlwm.attr.my_val() == 8

    hlwm.attr.my_val = '-=4'
    assert hlwm.attr.my_val() == 4

    hlwm.attr.my_val = '-=-2'
    assert hlwm.attr.my_val() == 6

    hlwm.attr.my_val = '+=-5'
    assert hlwm.attr.my_val() == 1

    hlwm.attr.my_val = '-=+1'
    assert hlwm.attr.my_val() == 0


def test_int_negative(hlwm):
    hlwm.call(['new_attr', 'int', 'my_val', '6'])
    hlwm.attr.my_val = '+=-20'
    assert hlwm.attr.my_val() == -14

    hlwm.attr.my_val = '-=-10'
    assert hlwm.attr.my_val() == -4


def test_uint_negative(hlwm):
    hlwm.call(['new_attr', 'uint', 'my_val', '19'])
    hlwm.attr.my_val = '+=-20'
    assert hlwm.attr.my_val() == 0

    hlwm.attr.my_val = '-=-10'
    assert hlwm.attr.my_val() == 10


def test_attr_type_for_many_types(hlwm):
    types = hlwm.complete(['mktemp'])
    assert len(types) >= 5

    for t in types:
        # also test that a newline is printed
        hlwm.call(['mktemp', t, 'ATTR', 'attr_type', 'ATTR']).stdout == t + '\\n'


def test_attr_type_invalid_arg(hlwm):
    hlwm.call_xfail('attr_type') \
        .expect_stderr('not enough arg')

    hlwm.call_xfail('attr_type foo bar') \
        .expect_stderr('Unknown .*bar')

    hlwm.call_xfail('attr_type not.an.attr') \
        .expect_stderr('No such object')
