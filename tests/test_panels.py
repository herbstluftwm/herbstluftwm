import pytest
from Xlib import Xatom


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
    ("pad_right", 30, (800 - 30, -2, 40, 11), [0, 20, 0, 0]),
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
