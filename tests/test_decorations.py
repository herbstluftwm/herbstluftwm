from herbstluftwm.types import Point
from conftest import RawImage
from conftest import HlwmBridge
import itertools
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
    x11.set_window_title(win_handle, title)
    # double check that hlwm has updated the client's title:
    winid = x11.winid_str(win_handle)
    hlwm = HlwmBridge.INSTANCE
    assert hlwm.attr.clients[winid].title() == title
    # then, take the screenshot:
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


@pytest.mark.parametrize("font", font_pool)
@pytest.mark.parametrize("ellipsis", [
    '',
    '...',
    '…',
    10 * 'a_very_long_string_that_takes_all_the_available_space',
])
def test_title_does_not_exceed_width(hlwm, x11, font, ellipsis):
    font_color = (255, 0, 0)  # a color available everywhere
    bw = 30
    hlwm.attr.theme.color = 'black'
    hlwm.attr.theme.title_color = RawImage.rgb2string(font_color)
    hlwm.attr.theme.title_height = 14
    hlwm.attr.theme.padding_top = 0
    hlwm.attr.theme.padding_left = 0
    hlwm.attr.theme.padding_right = 0
    hlwm.attr.theme.border_width = bw
    hlwm.attr.theme.title_font = font
    hlwm.attr.settings.ellipsis = ellipsis
    handle, winid = x11.create_client()

    # set a title that is too wide to be displayed in its entirety:
    w = hlwm.attr.clients[winid].decoration_geometry().width

    if font[0] != '-':
        three_bytes_per_glyph = 'ヘールブストルフト'
        assert len(three_bytes_per_glyph.encode('UTF-8')) == 3 * len(three_bytes_per_glyph)
        # for xft fonts, also test utf8 window titles
        utf8titles = [
            w * '♥',
            (w // 3) * 'äüöß',
            (w // len(three_bytes_per_glyph)) * three_bytes_per_glyph,
        ]
    else:
        # for plain X fonts, it does not seem to work in tox/pytest
        # (but strangely, it works in a manual Xephyr session)
        utf8titles = []

    for title in [w * '=', w * '|'] + utf8titles:
        img = screenshot_with_title(x11, handle, title)
        # verify that the title does not span too wide to the
        # left or to the right:
        # find leftmost non-black pixel:
        leftmost_font_x = None
        for x in range(0, w):
            for y in range(0, 14):  # only verify top `title_height`-many pixels
                if img.pixel(x, y) != (0, 0, 0):
                    leftmost_font_x = x
                    break
            if leftmost_font_x is not None:
                break
        # find rightmost non-black pixel:
        rightmost_font_x = None
        for x in range(w - 1, 0, -1):
            for y in range(0, 14):  # only verify top `title_height`-many pixels
                if img.pixel(x, y) != (0, 0, 0):
                    rightmost_font_x = x
                    break
            if rightmost_font_x is not None:
                break

        assert leftmost_font_x >= bw
        assert rightmost_font_x < bw + hlwm.attr.clients[winid].content_geometry().width


@pytest.mark.parametrize("font", font_pool)
def test_title_ellipsis_is_used(hlwm, x11, font):
    font_color = (255, 0, 0)  # a color available everywhere
    bw = 30
    hlwm.attr.theme.color = 'black'
    hlwm.attr.theme.title_color = RawImage.rgb2string(font_color)
    hlwm.attr.theme.title_height = 14
    hlwm.attr.theme.border_width = bw
    hlwm.attr.theme.title_font = font
    hlwm.attr.settings.ellipsis = 'abc'

    handle, winid = x11.create_client()
    assert screenshot_with_title(x11, handle, '   ').color_count(font_color) == 0
    # set a title that is too wide to be displayed in its entirety:
    w = hlwm.attr.clients[winid].decoration_geometry().width
    count1 = screenshot_with_title(x11, handle, w * ' ').color_count(font_color)
    assert count1 > 0
    hlwm.attr.settings.ellipsis = 'abcabc'
    count2 = screenshot_with_title(x11, handle, w * ' ').color_count(font_color)
    assert count2 > 0
    assert count2 == count1 * 2


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


@pytest.mark.parametrize("running_clients_num", [3])
def test_decoration_tab_colors(hlwm, x11, running_clients, running_clients_num):
    active_color = (200, 23, 0)  # something unique
    normal_color = (23, 200, 0)  # something unique
    hlwm.attr.theme.active.color = RawImage.rgb2string(active_color)
    hlwm.attr.theme.active.title_color = RawImage.rgb2string(active_color)
    hlwm.attr.theme.normal.color = RawImage.rgb2string(normal_color)
    hlwm.attr.theme.normal.title_color = RawImage.rgb2string(normal_color)

    hlwm.attr.theme.title_height = 20
    hlwm.call(['set_layout', 'max'])
    # split twice to make tab area smaller and screenshots faster :-)
    hlwm.call(['split', 'bottom'])
    hlwm.call(['split', 'bottom'])

    winhandle = x11.window(hlwm.attr.clients.focus.winid())
    img = x11.decoration_screenshot(winhandle)
    color_count = img.color_count_dict()
    # we have three tabs, and one of them should have the active color:
    assert color_count[active_color] * (running_clients_num - 1) == color_count[normal_color]

    # if we disable tabs, then the 'normal_color' should disappear:
    hlwm.attr.settings.tabbed_max = False
    winhandle = x11.window(hlwm.attr.clients.focus.winid())
    img = x11.decoration_screenshot(winhandle)
    new_color_count = img.color_count_dict()
    assert normal_color not in new_color_count
    assert new_color_count[active_color] == running_clients_num * color_count[active_color]


@pytest.mark.parametrize("running_clients_num", [3])
def test_decoration_tab_urgent(hlwm, x11, running_clients, running_clients_num):
    active_color = (200, 23, 0)  # something unique
    normal_color = (23, 200, 0)  # something unique
    urgent_color = (17, 2, 189)  # something unique
    hlwm.attr.theme.active.color = RawImage.rgb2string(active_color)
    hlwm.attr.theme.active.title_color = RawImage.rgb2string(active_color)
    hlwm.attr.theme.normal.color = RawImage.rgb2string(normal_color)
    hlwm.attr.theme.normal.title_color = RawImage.rgb2string(normal_color)
    hlwm.attr.theme.urgent.color = RawImage.rgb2string(urgent_color)
    hlwm.attr.theme.urgent.title_color = RawImage.rgb2string(urgent_color)

    hlwm.attr.theme.title_height = 20
    hlwm.call(['load', '(clients max:0 {})'.format(' '.join(running_clients))])
    # split twice to make tab area smaller and screenshots faster :-)
    hlwm.call(['split', 'bottom'])
    hlwm.call(['split', 'bottom'])

    winhandle = x11.window(running_clients[0])
    img = x11.decoration_screenshot(winhandle)
    color_count = img.color_count_dict()
    assert urgent_color not in color_count

    # make one of the unfocused tabs urgent
    x11.make_window_urgent(x11.window(running_clients[2]))
    img = x11.decoration_screenshot(winhandle)
    new_color_count = img.color_count_dict()
    assert urgent_color in new_color_count
    # there is one 'normal' and one 'urgent' tab, so the colors should
    # appear similarly often:
    assert new_color_count[urgent_color] == new_color_count[normal_color]
    assert new_color_count[normal_color] == color_count[normal_color] / 2


def test_decoration_tab_title_update(hlwm, x11):
    text_color = (212, 189, 140)
    hlwm.attr.theme.title_color = RawImage.rgb2string(text_color)
    hlwm.attr.theme.title_height = 20
    hlwm.call(['set_layout', 'max'])
    # split twice to make tab area smaller and screenshots faster :-)
    hlwm.call(['split', 'bottom'])
    hlwm.call(['split', 'bottom'])

    count = 5
    win_handles = [x11.create_client()[0] for _ in range(0, count)]

    # empty all window titles:
    for wh in win_handles:
        x11.set_window_title(wh, '')

    # focus handle 0:
    hlwm.call(['jumpto', x11.winid_str(win_handles[0])])

    # take a screenshot, it should not contain the text_color:
    assert x11.decoration_screenshot(win_handles[0]).color_count(text_color) == 0

    # change the title of an unfocused window:
    x11.set_window_title(win_handles[2], 'SOMETHING')
    # this change should now be visible in the tab bar, at least 5 pixels
    # should have this color now:
    assert x11.decoration_screenshot(win_handles[0]).color_count(text_color) > 5


@pytest.mark.parametrize("running_clients_num", [4])
def test_decoration_click_changes_tab(hlwm, mouse, running_clients, running_clients_num):
    hlwm.call(['load', '(clients max:0 {})'.format(' '.join(running_clients))])
    hlwm.attr.settings.tabbed_max = True
    hlwm.attr.theme.title_height = 10

    geo = hlwm.attr.clients.focus.decoration_geometry()
    tabbar_top_left = geo.topleft()
    tabbar_bottom_right = geo.topleft() + Point(geo.width, hlwm.attr.theme.title_height())
    for idx in reversed(range(0, running_clients_num)):
        # pick a point between top left corner of the title bar
        # and the bottom right corner of the title bar:
        # the extra 0.5 makes that we click in the middle of the tab
        ratio = (idx + 0.5) / running_clients_num
        cursor = tabbar_top_left * (1 - ratio) + tabbar_bottom_right * ratio
        mouse.move_to(cursor.x, cursor.y)
        mouse.click('1')

        assert hlwm.attr.clients.focus.winid() == running_clients[idx]


def test_decoration_click_into_window_does_not_change_tab(hlwm, mouse):
    wins = hlwm.create_clients(2)
    hlwm.call(['load', '(clients max:1 {})'.format(' '.join(wins))])
    hlwm.attr.settings.tabbed_max = True
    hlwm.attr.theme.title_height = 10

    assert hlwm.attr.clients.focus.winid() == wins[1]

    # move into the window and click:
    mouse.move_into(wins[0], x=4, y=4)
    mouse.click('1')

    # this does not change the focus:
    assert hlwm.attr.clients.focus.winid() == wins[1]

    # to double check:
    # if the cursor was 8px further up, the click
    # however would change the tab
    mouse.move_relative(0, -8)
    mouse.click('1')

    assert hlwm.attr.clients.focus.winid() == wins[0]


def test_textalign_completion(hlwm):
    """Test the TextAlign converter"""
    assert hlwm.complete(['attr', 'theme.title_align']) \
        == sorted(['left', 'right', 'center'])
    for k in hlwm.complete(['attr', 'theme.title_align']):
        hlwm.attr.theme.title_align = k
        assert hlwm.attr.theme.title_align() == k


@pytest.mark.parametrize("client_count", [1, 2])
def test_decoration_title_align(hlwm, x11, client_count):
    """test the title_align attribute,
    by computing the 'average' position of the title
    """
    text_color = (212, 189, 140)
    hlwm.attr.theme.title_color = RawImage.rgb2string(text_color)
    hlwm.attr.settings.tabbed_max = True
    hlwm.attr.theme.title_height = 10

    win_handle, winid = x11.create_client()
    hlwm.attr.tags.focus.tiling.focused_frame.algorithm = 'max'
    while hlwm.attr.tags.focus.client_count() < client_count:
        x11.create_client()

    assert hlwm.attr.clients.focus.winid() == winid
    x11.set_window_title(win_handle, '-')

    # compute the 'average' title position
    align_to_title_pos = {}
    for align in ['left', 'right', 'center']:
        hlwm.attr.theme.title_align = align
        img = x11.decoration_screenshot(win_handle)
        point_sum = Point(0, 0)
        point_count = 0
        for x, y in itertools.product(range(0, img.width), range(0, img.height)):
            if img.pixel(x, y) == text_color:
                point_sum.x += x
                point_sum.y += y
                point_count += 1
        # compute the average point:
        align_to_title_pos[align] = point_sum // point_count

    # all titles should be on the same height:
    assert align_to_title_pos['left'].y == align_to_title_pos['center'].y
    assert align_to_title_pos['center'].y == align_to_title_pos['right'].y

    # the x coordinate should be different by at least this:
    # the width of the decoration, divided by the number of tabs
    # and divided by roughly 3 :-)
    x_diff = hlwm.attr.clients.focus.decoration_geometry().width / client_count / 3
    assert align_to_title_pos['left'].x + x_diff < align_to_title_pos['center'].x
    assert align_to_title_pos['center'].x + x_diff < align_to_title_pos['right'].x
