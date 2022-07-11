import pytest


@pytest.mark.parametrize("running_clients_num", [0, 1, 2])
def test_show_frame_decorations_one_frame(
    hlwm, x11, running_clients, running_clients_num
):
    expected_frame_count = {
        "all": 1,
        "focused": 1,
        "focused_if_multiple": 1 if running_clients_num > 0 else 0,
        "nonempty": 1 if running_clients_num > 0 else 0,
        "if_multiple": 1 if running_clients_num > 0 else 0,
        "if_empty": 0 if running_clients_num > 0 else 1,
        "none": 0,
    }
    for v in hlwm.complete(["set", "show_frame_decorations"]):
        hlwm.attr.settings.show_frame_decorations = v
        assert len(x11.get_hlwm_frames(only_visible=True)) == expected_frame_count[v]


def test_show_frame_decorations_focus(hlwm, x11):
    winid, _ = hlwm.create_client()
    # create a split on the right, and focus the empty frame
    layout = f"""
    (split horizontal:0.5:1 (clients max:0 {winid}) (clients max:0))
    """
    hlwm.call(["load", layout.strip()])
    expected_frame_count = {
        "all": 2,
        "focused": 2,
        "focused_if_multiple": 2,
        "nonempty": 1,
        "if_multiple": 2,
        "if_empty": 1,
        "none": 0,
    }
    for v in hlwm.complete(["set", "show_frame_decorations"]):
        hlwm.attr.settings.show_frame_decorations = v
        assert len(x11.get_hlwm_frames(only_visible=True)) == expected_frame_count[v]
