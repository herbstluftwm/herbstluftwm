from conftest import RawImage
import pytest

font_pool = [
    '-*-fixed-medium-r-*-*-13-*-*-*-*-*-*-*',
    'Dejavu Sans:pixelsize=12:bold'
]


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
