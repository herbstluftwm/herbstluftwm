
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
