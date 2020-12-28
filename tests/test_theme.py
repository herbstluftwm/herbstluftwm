import pytest


def read_values(hlwm, attributes):
    """return a dict with the given attributes"""
    result = {}
    for a in attributes:
        result[a] = hlwm.get_attr(a)
    return result


def change_value_helper(hlwm, attribute):
    """fuzzily change the value of the given attribute to some value"""
    value = hlwm.get_attr(attribute)
    old_value = value
    if value == "black" or value == "#000000":
        value = "red"
    elif value.isdigit():
        value = int(value) + 1
    elif value == "":
        value = "test"
    else:
        assert value == "", "unknown value, don't know how to alter"
    hlwm.call(['set_attr', attribute, value])
    assert hlwm.get_attr(attribute) != old_value


def test_reset_theme_tiling(hlwm):
    """test theme.tiling.reset"""
    affected = [
        "theme.tiling.active.color",
        "theme.tiling.normal.border_width",
        "theme.tiling.urgent.padding_left",
    ]
    unaffected = [
        "theme.floating.active.color",
        # note that changing theme.color will
        # also change theme.tiling.active.inner_color
        # but, when calling theme.tiling.reset, theme.inner_color
        # must not be affected
        "theme.inner_color",
        "theme.normal.padding_right",
    ]
    # back up all the original values of all the attributes mentioned above
    affected_values = read_values(hlwm, affected)
    unaffected_values = read_values(hlwm, unaffected)
    # change the values of all these attributes
    for a in affected + unaffected:
        change_value_helper(hlwm, a)

    hlwm.call(['set_attr', 'theme.tiling.reset', '1'])

    # check attributes in 'affected' get reset to their old value
    for a in affected:
        assert hlwm.get_attr(a) == affected_values[a]
    # check that attributes in 'unaffected' do not get reset
    for a in unaffected:
        assert hlwm.get_attr(a) != unaffected_values[a]


def test_attr_propagate_same_value_twice(hlwm):
    # propagate some value
    hlwm.call('set_attr theme.tiling.border_width 4')
    # change the value in the propagation target
    hlwm.call('set_attr theme.tiling.active.border_width 2')
    assert hlwm.get_attr('theme.tiling.active.border_width') == '2'

    # now propagate the same value again
    hlwm.call('set_attr theme.tiling.border_width 4')

    # now the propagation target must be updated again
    assert hlwm.get_attr('theme.tiling.active.border_width') == '4'


@pytest.mark.parametrize("reset", [True, False])
def test_minimal_theme(hlwm, reset):
    if reset:
        hlwm.call('set_attr theme.minimal.active.border_width 4')
        hlwm.call('set_attr theme.minimal.reset 1')
    for scheme in ['active', 'normal', 'urgent']:
        assert hlwm.get_attr(f'theme.minimal.{scheme}.border_width') == '0'


@pytest.mark.parametrize("tight_dec", [True, False])
def test_tight_decoration(hlwm, tight_dec):
    hlwm.call('set_attr theme.tight_decoration {}'.format(hlwm.bool(tight_dec)))
    # it's not easy to test the tight decration, so we only
    # check that it does not crash
    hlwm.create_client()
    hlwm.call('split explode')


def test_font_type_existing_font(hlwm):
    for value in ['*FIXED*', '*FiXed*', 'fixed']:
        hlwm.call(['set_attr', 'theme.title_font', value])

        assert hlwm.attr.theme.font() == value


def test_font_type_non_existing_font(hlwm):
    value = 'Some long font name that hopefully does not exist'
    hlwm.call_xfail(['set_attr', 'theme.title_font', value]) \
        .expect_stderr(f"cannot allocate font.*'{value}'")
