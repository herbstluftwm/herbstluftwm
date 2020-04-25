import pytest
import test_stack


@pytest.mark.parametrize("single_floating", [True, False])
@pytest.mark.parametrize("raise_on_click", [True, False])
def test_focus_on_click(hlwm, mouse, raise_on_click, single_floating):
    if single_floating:
        hlwm.call('rule floating=on')
    else:
        hlwm.call('set_attr tags.focus.floating on')
    hlwm.call(['set', 'raise_on_click', hlwm.bool(raise_on_click)])
    c1, _ = hlwm.create_client(position=(0, 0))
    c2, _ = hlwm.create_client(position=(300, 0))
    hlwm.call(f'jumpto {c2}')  # also raises c2
    assert hlwm.get_attr('clients.focus.winid') == c2
    assert test_stack.helper_get_stack_as_list(hlwm) == [c2, c1]

    mouse.click('1', into_win_id=c1)

    assert hlwm.get_attr('clients.focus.winid') == c1
    stack = test_stack.helper_get_stack_as_list(hlwm)
    if raise_on_click:
        # c1 gets back on top
        assert stack == [c1, c2]
    else:
        # c2 stays on top
        assert stack == [c2, c1]


@pytest.mark.parametrize("single_floating", [True, False])
@pytest.mark.parametrize("focus_follows_mouse", [True, False])
def test_focus_follows_mouse(hlwm, mouse, focus_follows_mouse, single_floating):
    if single_floating:
        hlwm.call('rule floating=on')
    else:
        hlwm.call('set_attr tags.focus.floating on')
    hlwm.call('set_attr tags.focus.floating on')
    hlwm.call(['set', 'focus_follows_mouse', hlwm.bool(focus_follows_mouse)])
    c1, _ = hlwm.create_client(position=(0, 0))
    c2, _ = hlwm.create_client(position=(300, 0))
    hlwm.call(f'jumpto {c2}')  # also raises c2
    assert hlwm.get_attr('clients.focus.winid') == c2
    assert test_stack.helper_get_stack_as_list(hlwm) == [c2, c1]

    mouse.move_into(c1)

    c1_is_focused = hlwm.get_attr('clients.focus.winid') == c1
    # c1 is focused iff focus_follows_mouse was activated
    assert c1_is_focused == focus_follows_mouse
    # stacking is unchanged
    assert test_stack.helper_get_stack_as_list(hlwm) == [c2, c1]


@pytest.mark.parametrize("click", [True, False])
@pytest.mark.parametrize("focus_follows_mouse", [True, False])
def test_focus_frame_by_mouse(hlwm, mouse, click, focus_follows_mouse):
    hlwm.call('set always_show_frame on')
    hlwm.call('set frame_bg_transparent off')
    hlwm.call(['set', 'focus_follows_mouse', hlwm.bool(focus_follows_mouse)])
    layout_template = '(split horizontal:0.5:{}'
    layout_template += ' (clients vertical:0)'
    layout_template += ' (clients vertical:0))'
    # a layout where the left frame is focused
    hlwm.call(['load', layout_template.format(0)])
    assert layout_template.format(0) == hlwm.call('dump').stdout
    width, height = [int(v) for v in hlwm.call('monitor_rect').stdout.split(' ')][2:]

    # go to left frame
    mouse.move_to(width * 0.25, height * 0.5)

    # go to right frame
    mouse.move_to(width * 0.75, height * 0.5)
    if click:
        mouse.click('1')

    # we expect the focus to go to the second frame
    # if focus_follows_mouse is enabled or if we click explicitly
    expected_focus = 1 if focus_follows_mouse or click else 0
    assert layout_template.format(expected_focus) \
        == hlwm.call('dump').stdout
