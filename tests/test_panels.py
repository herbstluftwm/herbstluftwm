import pytest
from Xlib import Xatom


class NET_WM_STRUT_PARTIAL:
    # the enum from _NET_WM_STRUT_PARTIAL in EWMH
    # https://specifications.freedesktop.org/wm-spec/wm-spec-latest.html#idm45075509080400
    left = 0
    right = 1
    top = 2
    bottom = 3
    left_start_y = 4
    left_end_y = 5
    right_start_y = 6
    right_end_y = 7
    top_start_x = 8
    top_end_x = 9
    bottom_start_x = 10
    bottom_end_x = 11


@pytest.mark.parametrize("which_pad, pad_size, geometry", [
    ("pad_left", 10, (-1, 0, 11, 400)),
    ("pad_right", 20, (800 - 20, 23, 20, 400)),
    ("pad_up", 40, (-5, 0, 805, 40)),
    ("pad_down", 15, (4, 600 - 15, 200, 40)),
    # cases where no pad is applied because the width/height ratio is ambiguous:
    ("pad_left", 0, (0, 0, 40, 40)),
    ("pad_down", 0, (800 - 40, 600 - 40, 40, 40)),
])
def test_panel_based_on_intersection(hlwm, x11, which_pad, pad_size, geometry):
    # monitor 0 is affected by the panel, but the other monitor is not
    hlwm.call('add othertag')
    hlwm.call('set_monitors 800x600+0+0 800x600+800+0')
    winhandle, _ = x11.create_client(geometry=geometry,
                                     window_type='_NET_WM_WINDOW_TYPE_DOCK')

    assert int(hlwm.attr.monitors[0][which_pad]()) == pad_size
    for p in ["pad_left", "pad_right", "pad_up", "pad_down"]:
        assert hlwm.attr.monitors[1][p]() == '0', \
            "monitor 1 must never be affected"
        if p == which_pad:
            continue
        assert hlwm.attr.monitors[0][p]() == '0'


@pytest.mark.parametrize("which_pad, pad_size, geometry, wm_strut", [
    ("pad_left", 20, (-1, 0, 21, 4), [20, 0, 0, 0]),
    ("pad_right", 35, (800 - 30, -2, 30, 11), [0, 35, 0, 0]),
    ("pad_up", 40, (0, 0, 30, 40), [0, 0, 40, 0]),
    ("pad_down", 15, (80, 600 - 15, 2, 15), [0, 0, 0, 15]),
])
@pytest.mark.parametrize("strut_before_map", [True, False])
def test_panel_based_on_wmstrut(hlwm, x11, which_pad, pad_size, geometry, wm_strut, strut_before_map):
    # monitor 0 is affected by the panel, but the other monitor is not
    hlwm.call('add othertag')
    hlwm.call('set_monitors 800x600+0+0 800x600+800+0')

    def set_wm_strut(winhandle):
        xproperty = x11.display.intern_atom('_NET_WM_STRUT')
        winhandle.change_property(xproperty, Xatom.CARDINAL, 32, wm_strut)

    def noop(winhandle):
        pass

    winhandle, _ = x11.create_client(geometry=geometry,
                                     window_type='_NET_WM_WINDOW_TYPE_DOCK',
                                     pre_map=set_wm_strut if strut_before_map else noop,
                                     )
    if not strut_before_map:
        set_wm_strut(winhandle)
        # sync hlwm and x11
        x11.display.sync()
        hlwm.call('true')

    assert int(hlwm.attr.monitors[0][which_pad]()) == pad_size
    for p in ["pad_left", "pad_right", "pad_up", "pad_down"]:
        assert hlwm.attr.monitors[1][p]() == '0', \
            "monitor 1 must never be affected"
        if p == which_pad:
            continue
        assert hlwm.attr.monitors[0][p]() == '0'


def test_panel_wm_strut_partial_on_big_screen(hlwm, x11):
    """This reproduces the issue in
    https://github.com/herbstluftwm/herbstluftwm/issues/1110
    where the panel intersects with the top of the monitor but not at
    y-coordinate 0
    """
    hlwm.call('set_monitors 1920x1200')
    hlwm.call('set auto_detect_panels true')

    def set_wm_strut(winhandle):
        xproperty = x11.display.intern_atom('_NET_WM_STRUT')
        winhandle.change_property(xproperty, Xatom.CARDINAL, 32, [0, 0, 54, 0])
        xproperty = x11.display.intern_atom('_NET_WM_STRUT_PARTIAL')
        winhandle.change_property(xproperty, Xatom.CARDINAL, 32,
                                  [0, 0, 54, 0, 0, 0, 0, 0, 192, 1919, 0, 0])

    winhandle, _ = x11.create_client(geometry=(192, 12, 1536, 42),
                                     window_type='_NET_WM_WINDOW_TYPE_DOCK',
                                     pre_map=set_wm_strut,
                                     )

    assert hlwm.call('list_padding').stdout.strip() == '54 0 0 0'


@pytest.mark.parametrize("xvfb", [(1280 + 1024, 1024)], indirect=True)
def test_panel_wm_strut_partial_different_sized_screens(hlwm, x11):
    """This is the xinerama example from the EWMH doc on _NET_WM_STRUT_PARTIAL
    https://specifications.freedesktop.org/wm-spec/wm-spec-latest.html#idm45075509080400

    »Assume that the set up uses two monitors, one running at 1280x1024 and the
    other to the right running at 1024x768, with the top edge of the two
    physical displays aligned. If the panel wants to fill the entire bottom
    edge of the smaller display with a panel 50 pixels tall, it should set a
    bottom strut of 306, with bottom_start_x of 1280, and bottom_end_x of 2303.
    Note that the strut is relative to the screen edge, and not the edge of the
    xinerama monitor.«
    """
    hlwm.call('add anothertag')
    hlwm.call('set_monitors 1280x1024+0+0 1024x768+1280+0')
    hlwm.call('set auto_detect_panels true')

    def set_wm_strut(winhandle):
        xproperty = x11.display.intern_atom('_NET_WM_STRUT_PARTIAL')
        values = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]
        values[NET_WM_STRUT_PARTIAL.bottom] = 306
        values[NET_WM_STRUT_PARTIAL.bottom_start_x] = 1280
        values[NET_WM_STRUT_PARTIAL.bottom_end_x] = 2303
        winhandle.change_property(xproperty, Xatom.CARDINAL, 32, values)

    winhandle, _ = x11.create_client(geometry=(192, 12, 1536, 42),
                                     window_type='_NET_WM_WINDOW_TYPE_DOCK',
                                     pre_map=set_wm_strut,
                                     )

    assert hlwm.call('list_padding 0').stdout.strip() == '0 0 0 0'
    assert hlwm.call('list_padding 1').stdout.strip() == '0 0 50 0'
