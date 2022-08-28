import pytest
from herbstluftwm.types import Point

TITLE_WHEN_VALUES = ['always', 'never', 'one_tab', 'multiple_tabs']


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

        assert hlwm.attr.theme.title_font() == value


def test_font_type_non_existing_font(hlwm):
    value = '-Some long font name that hopefully does not exist'
    hlwm.call_xfail(['set_attr', 'theme.title_font', value]) \
        .expect_stderr(f"(cannot allocate font.*'{value}'|{value}.*The following charsets are unknown)")


@pytest.mark.parametrize("floating", [True, False])
def test_decoration_geometry_vs_content_geometry(hlwm, floating):
    hlwm.attr.tags.focus.floating = floating
    hlwm.attr.theme.border_width = 1
    hlwm.attr.theme.title_height = 12
    hlwm.attr.theme.padding_left = 2
    hlwm.attr.theme.padding_right = 3
    hlwm.attr.theme.padding_top = 4
    hlwm.attr.theme.padding_bottom = 5

    winid, _ = hlwm.create_client()

    content = hlwm.attr.clients[winid].content_geometry()
    decoration = hlwm.attr.clients[winid].decoration_geometry()

    assert decoration.topleft() + Point(1 + 2, 1 + 4 + 12) == content.topleft()
    assert content.bottomright() + Point(1 + 3, 1 + 5) == decoration.bottomright()


@pytest.mark.parametrize("floating", [True, False])
def test_decoration_geometry_vs_x11_data(hlwm, x11, floating):
    hlwm.attr.tags.focus.floating = floating

    handle, winid = x11.create_client()
    hlwm.attr.clients[winid].floating = floating

    geoX11 = x11.get_absolute_geometry(x11.get_decoration_window(handle))
    geoAttr = hlwm.attr.clients[winid].decoration_geometry()

    assert geoAttr == geoX11


def test_title_when_parsing(hlwm):
    assert sorted(TITLE_WHEN_VALUES) == sorted(hlwm.complete(['attr', 'theme.title_when']))

    for v in TITLE_WHEN_VALUES:
        hlwm.attr.theme.title_when = v
        assert hlwm.attr.theme.title_when() == v

    hlwm.call_xfail('attr theme.title_when foobar') \
        .expect_stderr('Expecting one of:.*always')


def test_title_when_behaviour(hlwm):
    hlwm.attr.theme.title_height = 10
    hlwm.attr.theme.title_depth = 5
    expected_title_size = 10 + 5
    hlwm.attr.theme.padding_top = 4
    bw = 3
    hlwm.attr.theme.border_width = bw
    hlwm.attr.tags.focus.tiling.focused_frame.algorithm = 'max'

    for client_count in range(1, 4):
        hlwm.create_client()

        for title_when in TITLE_WHEN_VALUES:
            hlwm.attr.theme.title_when = title_when

            content_geo = hlwm.attr.clients.focus.content_geometry()
            decoration_geo = hlwm.attr.clients.focus.decoration_geometry()

            assert content_geo.bottomright() + Point(bw, bw) == decoration_geo.bottomright()
            assert decoration_geo.x + bw == content_geo.x
            title_size = content_geo.y - decoration_geo.y - bw - hlwm.attr.theme.padding_top()

            title_expected = title_when == 'always' \
                or (title_when == 'one_tab' and client_count >= 1) \
                or (title_when == 'multiple_tabs' and client_count >= 2)

            if title_expected:
                assert expected_title_size > 0
                assert title_size == expected_title_size
            else:
                assert title_size == 0


@pytest.mark.parametrize("floating", [True, False])
def test_title_when_for_absence_of_tabs(hlwm, floating):
    """
    for floating clients or frames with an algorithm other than 'max',
    there should be no tabs at all and thus the titlebar is
    only shown if 'title_when' is 'always'.
    """
    hlwm.attr.theme.title_height = 10
    hlwm.attr.theme.title_depth = 5
    expected_title_size = 10 + 5
    hlwm.attr.theme.padding_top = 4
    bw = 3
    hlwm.attr.theme.border_width = bw
    hlwm.attr.tags.focus.floating = floating
    hlwm.attr.tags.focus.tiling.focused_frame.algorithm = 'vertical'
    for client_count in range(1, 4):
        hlwm.create_client()

        for title_when in TITLE_WHEN_VALUES:
            hlwm.attr.theme.title_when = title_when
            content_geo = hlwm.attr.clients.focus.content_geometry()
            decoration_geo = hlwm.attr.clients.focus.decoration_geometry()

            assert content_geo.bottomright() + Point(bw, bw) == decoration_geo.bottomright()
            assert decoration_geo.x + bw == content_geo.x
            title_size = content_geo.y - decoration_geo.y - bw - hlwm.attr.theme.padding_top()
            if title_when == 'always':
                assert title_size == expected_title_size
            else:
                assert title_size == 0


def test_tabs_cleared_in_floating(hlwm, x11):
    """
    If a client switches from tiling to floating, then its
    tab list must be cleared early enough such that title_when
    evaluates correctly.

    Unfortunately, this bug could not be reproduced with xterm because for some
    reason, xterm triggers multiple redraws and then 'title_when' converges
    quickly enough.

    Tests https://github.com/herbstluftwm/herbstluftwm/issues/1435
    """
    hlwm.call('rule floatplacement=none focus=on')
    hlwm.attr.theme.title_when = 'multiple_tabs'
    hlwm.attr.theme.title_height = 10
    hlwm.call('set_layout max')

    # it is important here, that the client does not trigger multiple
    # resize events, so xterm is not suitable here.
    c1, w1 = x11.create_client()
    c2, w2 = x11.create_client()

    hlwm.attr.tags.focus.floating = True

    assert hlwm.attr.clients[w1].floating_geometry() \
        == hlwm.attr.clients[w1].content_geometry()

    assert hlwm.attr.clients[w2].floating_geometry() \
        == hlwm.attr.clients[w2].content_geometry()


def test_font_not_empty(hlwm):
    hlwm.call_xfail("set_attr theme.title_font ''") \
        .expect_stderr("unknown font description")
