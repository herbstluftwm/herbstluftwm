import pytest
from Xlib import X
import os
import conftest


@pytest.mark.parametrize("running_clients_num", [0, 1, 2])
def test_window_focus(hlwm, x11, hc_idle, running_clients_num):
    # before creating the clients, assert that new clients are focused
    hlwm.call('rule focus=on')

    # create some clients:
    last_client = None
    for k in range(running_clients_num):
        last_client, _ = x11.create_client()

    hlwm.call('true')  # sync with hlwm
    x11.display.sync()

    assert x11.ewmh.getActiveWindow() == last_client
    if last_client is None:
        # if no window is focused, some dummy window is focused. in the case of hlwm
        # we focus the window that is also used for the EWMH _NET_SUPPORTING_WM_CHECK.
        # (this is also what openbox is doing)
        assert x11.display.get_input_focus().focus.id == x11.get_property('_NET_SUPPORTING_WM_CHECK')[0]
        assert 'focus' not in hlwm.list_children('clients')
    else:
        hlwm.call('attr clients')
        print(hc_idle.hooks())
        print(x11.display.get_input_focus().focus)
        assert x11.display.get_input_focus().focus == last_client
        assert x11.winid_str(last_client) == hlwm.get_attr('clients.focus.winid')


def test_client_moveresizes_itself(hlwm, x11):
    # create a floating window
    hlwm.call('move_monitor 0 500x600+12+13 14 15 16 17')
    hlwm.call('floating on')
    hlwm.call('set_attr theme.border_width 0')
    # FIXME: why doesn't this work with sync_hlwm=True?
    w, _ = x11.create_client(geometry=(25, 26, 27, 28), sync_hlwm=False)

    # resize the window to some other geometry
    w.configure(x=60, y=70, width=300, height=200)
    x11.display.sync()

    hlwm.call('true')  # sync with hlwm
    # check that w now has the desired geometry
    win_geo = w.get_geometry()
    assert (win_geo.width, win_geo.height) == (300, 200)
    x, y = x11.get_absolute_top_left(w)
    assert (x, y) == (60, 70)


def test_focus_steal_via_xsetinputfocus(hlwm, x11):
    oldfocus, old_id = x11.create_client()
    newfocus, new_id = x11.create_client()

    hlwm.call(['jumpto', old_id])
    assert hlwm.attr.clients.focus.winid() == old_id

    # steal the focus like XSetInputFocus in `xdotool windowfocus` would do it:
    x11.display.set_input_focus(newfocus, X.RevertToParent, X.CurrentTime)
    x11.display.sync()

    assert hlwm.attr.clients.focus.winid() == new_id


class FakeCompositor:
    """
    This is not a compositing manager at all, but it claims to by
    taking the ownership of the corresponding x11 selection.
    """

    def __init__(self, x11):
        self.x11 = x11
        self.window = None

    def is_active(self):
        return self.window is not None

    @staticmethod
    def rgb_premultiply_alpha(rgb_tuple, alpha):
        return tuple([int((v * alpha) / 256) for v in rgb_tuple])

    def set_active(self, active):
        if self.is_active() != active:
            if self.window is None:
                self.window = self.x11.create_client(force_unmanage=True, sync_hlwm=False)[0]
                # just assume we are on screen number 0:
                screen_no = self.x11.display.get_default_screen()
                atom = self.x11.display.intern_atom(f'_NET_WM_CM_S{screen_no}')
                self.window.set_selection_owner(atom, X.CurrentTime)
                self.x11.display.sync()
            else:
                self.window.destroy()
                self.window = None
                self.x11.display.sync()


@pytest.mark.parametrize("with_compositor", [True, False])
def test_compositing_manager_detection_on_startup(hlwm_spawner, x11, xvfb, with_compositor):
    compositor = FakeCompositor(x11)
    if with_compositor:
        compositor.set_active(True)
    else:
        # start and shut down again
        compositor.set_active(True)
        compositor.set_active(False)

    hlwm_proc = hlwm_spawner()
    hlwm = conftest.HlwmBridge(os.environ['DISPLAY'], hlwm_proc)

    hlwm.attr.tags.focus.floating = True
    handle, _ = x11.create_client()

    hlwm.attr.theme.color = '#84cc4019'
    color_plain = (0x84, 0xcc, 0x40)
    color_premultiplied = FakeCompositor.rgb_premultiply_alpha(color_plain, 0x19)
    hlwm.attr.theme.border_width = 10
    hlwm.attr.theme.outer_width = 0

    img = x11.decoration_screenshot(handle)
    # just pick any pixel from the decoration, it shouldn't matter which:
    for x in range(0, 8):
        for y in range(0, 8):
            assert img.pixel(0, 0) == img.pixel(x, y)

    if with_compositor:
        assert img.pixel(0, 0) == color_premultiplied
    else:
        assert img.pixel(0, 0) == color_plain


def test_compositing_manager_detection_at_runtime(hlwm, x11):
    hlwm.attr.tags.focus.floating = True
    handle, _ = x11.create_client()

    hlwm.attr.theme.color = '#84cc4019'
    color_plain = (0x84, 0xcc, 0x40)
    color_premultiplied = FakeCompositor.rgb_premultiply_alpha(color_plain, 0x19)
    hlwm.attr.theme.border_width = 10
    hlwm.attr.theme.outer_width = 0

    compositor = FakeCompositor(x11)

    for active in [True, False, True, False]:
        compositor.set_active(active)
        x11.sync_with_hlwm()
        img = x11.decoration_screenshot(handle)
        # just pick any pixel from the decoration, it shouldn't matter which:
        for x in range(0, 8):
            for y in range(0, 8):
                assert img.pixel(0, 0) == img.pixel(x, y)
        if active:
            assert img.pixel(0, 0) == color_premultiplied
        else:
            assert img.pixel(0, 0) == color_plain
