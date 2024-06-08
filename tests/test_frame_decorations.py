import pytest
from conftest import PROCESS_SHUTDOWN_TIME


@pytest.mark.parametrize("running_clients_num", [0, 1, 2])
def test_show_frame_decorations_one_frame(hlwm, x11, running_clients, running_clients_num):
    expected_frame_count = {
        'all': 1,
        'focused': 1,
        'focused_if_multiple': 1 if running_clients_num > 0 else 0,
        'nonempty': 1 if running_clients_num > 0 else 0,
        'if_multiple': 1 if running_clients_num > 0 else 0,
        'if_empty': 0 if running_clients_num > 0 else 1,
        'none': 0,
    }
    for v in hlwm.complete(['set', 'show_frame_decorations']):
        hlwm.attr.settings.show_frame_decorations = v
        assert len(x11.get_hlwm_frames(only_visible=True)) == expected_frame_count[v]


def test_show_frame_decorations_focus(hlwm, x11):
    winid, _ = hlwm.create_client()
    # create a split on the right, and focus the empty frame
    layout = f"""
    (split horizontal:0.5:1 (clients max:0 {winid}) (clients max:0))
    """
    hlwm.call(['load', layout.strip()])
    expected_frame_count = {
        'all': 2,
        'focused': 2,
        'focused_if_multiple': 2,
        'nonempty': 1,
        'if_multiple': 2,
        'if_empty': 1,
        'none': 0,
    }
    for v in hlwm.complete(['set', 'show_frame_decorations']):
        hlwm.attr.settings.show_frame_decorations = v
        assert len(x11.get_hlwm_frames(only_visible=True)) == expected_frame_count[v]


def test_frame_bg_transparency_area_bug(hlwm, x11, mouse):
    """Reproduce Issue #1576"""
    hlwm.call(['set', 'frame_bg_transparent', 'off'])
    hlwm.call(['set', 'always_show_frame', 'on'])
    # put two frames side by side:
    hlwm.call(['split', 'right', 0.5])
    # put the mouse cursor on the right-hand frame
    screen_size = hlwm.attr.monitors[0].geometry()
    mouse.move_to(int(0.75 * screen_size.width), int(0.5 * screen_size.height))
    # the focus is on the left frame. Place a client there:
    winid, proc = hlwm.create_client()
    hlwm.call(['resize', 'left', '+0.05'])
    # then focus the right-hand frame
    hlwm.call(['focus', 'right'])

    # remove the client
    hlwm.call(['close', winid])
    proc.wait(PROCESS_SHUTDOWN_TIME)

    # remove the right hand frame
    hlwm.call(['remove'])

    # now, the formerly left hand frame should span
    # the whole screen. So lets see which window is under the
    # mouse cursor:
    window = x11.get_window_under_cursor()
    # it should not be the root window
    assert window != x11.root
    # but the (only) frame decoration instead:
    hlwm_frames = x11.get_hlwm_frames()
    assert len(hlwm_frames) == 1
    assert hlwm_frames[0] == window
