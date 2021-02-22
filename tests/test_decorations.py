from conftest import RawImage
import pytest

font_pool = [
    '-*-fixed-medium-r-*-*-13-*-*-*-*-*-*-*',
    'Dejavu Sans:pixelsize=12:bold'
]


@pytest.mark.parametrize("xvfb", [{'xrender': v} for v in [True, False]], indirect=True)
@pytest.mark.parametrize("hlwm_process", [{'transparency': v} for v in [True, False]], indirect=True)
def test_window_border_plain(hlwm, x11):
    color = (0x9f, 0xbc, 0x12)
    bw = 5  # border width
    handle, _ = x11.create_client()
    hlwm.attr.theme.color = RawImage.rgb2string(color)
    hlwm.attr.theme.border_width = bw
    img = x11.decoration_screenshot(handle)
    assert img.pixel(0, 0) == color
    expected_count = 2 * bw * img.width  # horizontal border line
    expected_count += 2 * bw * img.height  # vertical border
    expected_count -= 4 * bw * bw  # we counted each twice
    assert img.color_count(color) == expected_count


@pytest.mark.parametrize("xvfb", [{'xrender': v} for v in [True, False]], indirect=True)
@pytest.mark.parametrize("hlwm_process", [{'transparency': v} for v in [True, False]], indirect=True)
def test_window_border_inner(hlwm, x11):
    color = (239, 2, 190)
    bw = 5  # border width
    inner_color = (48, 225, 26)
    inner_bw = 2
    hlwm.attr.theme.color = RawImage.rgb2string(color)
    hlwm.attr.theme.border_width = bw
    hlwm.attr.theme.inner_color = RawImage.rgb2string(inner_color)
    hlwm.attr.theme.inner_width = inner_bw
    handle, _ = x11.create_client()
    img = x11.decoration_screenshot(handle)
    # we check the inner border color in the upper left corner
    for x in range(0, bw):
        for y in range(0, bw):
            threshold = bw - inner_bw
            expected_color = inner_color if x >= threshold and y >= threshold else color
            assert img.pixel(x, y) == expected_color


@pytest.mark.parametrize("xvfb", [{'xrender': v} for v in [True, False]], indirect=True)
@pytest.mark.parametrize("hlwm_process", [{'transparency': v} for v in [True, False]], indirect=True)
def test_window_border_outer(hlwm, x11):
    color = (239, 2, 190)
    bw = 6  # border width
    outer_color = (48, 225, 26)
    outer_bw = 3
    hlwm.attr.theme.color = RawImage.rgb2string(color)
    hlwm.attr.theme.border_width = bw
    hlwm.attr.theme.outer_color = RawImage.rgb2string(outer_color)
    hlwm.attr.theme.outer_width = outer_bw
    handle, _ = x11.create_client()
    img = x11.decoration_screenshot(handle)
    # check the upper left corner
    for x in range(0, bw):
        for y in range(0, bw):
            threshold = outer_bw
            expected_color = outer_color if x < threshold or y < threshold else color
            assert img.pixel(x, y) == expected_color


def screenshot_with_title(x11, win_handle, title):
    """ set the win_handle's window title and then
    take a screenshot
    """
    win_handle.set_wm_name(title)
    x11.sync_with_hlwm()
    return x11.decoration_screenshot(win_handle)


@pytest.mark.parametrize("font", font_pool)
def test_title_every_letter_is_drawn(hlwm, x11, font):
    """the number of letters has some effect"""
    font_color = (255, 0, 0)  # a color available everywhere
    hlwm.attr.theme.color = 'black'
    hlwm.attr.theme.title_color = RawImage.rgb2string(font_color)
    hlwm.attr.theme.title_height = 14
    hlwm.attr.theme.padding_top = 4
    hlwm.attr.theme.title_font = font
    handle, _ = x11.create_client()

    # set the window title to some word
    count1 = screenshot_with_title(x11, handle, 'test').color_count(font_color)

    # duplicate the word in the title
    count2 = screenshot_with_title(x11, handle, 'test test').color_count(font_color)

    # then the number of pixels of the font_color should have doubled:
    assert count1 != 0
    assert count1 * 2 == count2


@pytest.mark.parametrize("font", font_pool)
def test_title_different_letters_are_drawn(hlwm, x11, font):
    """changing letters changes the image"""
    font_color = (255, 0, 0)  # a color available everywhere
    hlwm.attr.theme.color = 'black'
    hlwm.attr.theme.title_color = RawImage.rgb2string(font_color)
    hlwm.attr.theme.title_height = 14
    hlwm.attr.theme.padding_top = 4
    hlwm.attr.theme.title_font = font
    handle, _ = x11.create_client()

    # put some characters in the title that take only few pixels
    count1 = screenshot_with_title(x11, handle, ',.b').color_count(font_color)

    # alter characters to others taking more pixels
    count2 = screenshot_with_title(x11, handle, ';:B').color_count(font_color)

    # then the number of pixels should have increased
    assert count1 < count2


@pytest.mark.parametrize("frame_bg_transparent", ['on', 'off'])
def test_frame_bg_transparent(hlwm, x11, frame_bg_transparent):
    hlwm.attr.settings.frame_gap = 24  # should not matter
    hlwm.attr.settings.frame_border_width = 0
    hlwm.attr.settings.frame_bg_active_color = '#ef0000'
    hlwm.attr.settings.frame_bg_transparent = frame_bg_transparent
    tw = 8
    hlwm.attr.settings.frame_transparent_width = tw

    [frame_win] = x11.get_hlwm_frames()
    img = x11.screenshot(frame_win)
    w = img.width
    h = img.height

    for x, y in [(2, 2), (4, 2), (2, 8), (3, 4), (7, 7), (w - 1, h - 1), (w - tw, h - tw)]:
        assert img.pixel(x, y) == (0xef, 0, 0), \
            f"pixel at {x}, {y}"

    # if there is a hole in the frame decoration, it seems that black is used
    # (either as a default value or because that's the color of the root window)
    color_expected = (0, 0, 0) if frame_bg_transparent == 'on' else (0xef, 0, 0)
    for x, y in [(tw, tw), (tw, tw + 2), (w - tw - 1, h - tw - 1), (50, h - tw - 1), (w // 2, h // 2)]:
        assert img.pixel(x, y) == color_expected, \
            f"pixel at {x}, {y}"


@pytest.mark.parametrize("frame_bg_transparent", ['on', 'off'])
def test_frame_holes_for_tiled_client(hlwm, x11, frame_bg_transparent):
    hlwm.attr.settings.frame_bg_active_color = '#efcd32'
    hlwm.attr.settings.frame_bg_transparent = frame_bg_transparent
    hlwm.attr.settings.frame_transparent_width = 8

    def expect_frame_bg_color(winid, expected_color):
        img = x11.screenshot(frame_win)
        w = img.width
        h = img.height
        for x, y in [(0, 0), (0, h - 1), (w - 1, 0), (w - 1, h - 1)]:
            assert img.pixel(x, y) == expected_color, \
                f"pixel at {x}, {y}"

    [frame_win] = x11.get_hlwm_frames()
    expect_frame_bg_color(frame_win, (0xef, 0xcd, 0x32))

    x11.create_client()

    # one big tiled client should hide all of the frames bg color:
    expect_frame_bg_color(frame_win, (0, 0, 0))


@pytest.mark.parametrize("frame_bg_transparent", ['on', 'off'])
def test_frame_holes_for_pseudotiled_client(hlwm, x11, frame_bg_transparent):
    bgcol = (0xef, 0xcd, 0x32)
    hlwm.attr.settings.frame_bg_active_color = RawImage.rgb2string(bgcol)
    hlwm.attr.settings.frame_bg_transparent = frame_bg_transparent
    hlwm.attr.settings.frame_transparent_width = 8

    [frame_win] = x11.get_hlwm_frames()
    geo = frame_win.get_geometry()
    w = geo.width
    h = geo.height

    # create a pseudotiled client that is very wide but not very high:
    winhandle, winid = x11.create_client(geometry=(0, 0, w + 10, h // 3 - 10))
    hlwm.attr.clients[winid].pseudotile = 'on'

    img = x11.screenshot(frame_win)
    assert (img.width, img.height) == (w, h)

    # the frame is visible on the top and bottom
    img.pixel(0, 0) == bgcol
    img.pixel(0, h - 1) == bgcol
    img.pixel(w // 2, 0) == bgcol
    img.pixel(w // 2, h - 1) == bgcol

    # but the frame is not visible on the left and right
    black = (0, 0, 0)
    img.pixel(0, h // 2) == black
    img.pixel(w - 1, h // 2) == black
    img.pixel(w // 2, h // 2) == black
