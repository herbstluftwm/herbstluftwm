import pytest
import re


@pytest.mark.parametrize("invalid_layout,error_pos", [
    ('(', 1),
    ('()', 1),
    ('foo baar', 0),
    ('(foo baar', 1),
    ('((clients max:0 ))', 1),
    ('(clients)', 8),
    ('(clients )', 9),
    ('(split max:0.5:1)', 7),
    ('(split horizontal:0.05:1)', 7),
    ('(split horizontal:0.95:1)', 7),
    ('(split horizontal:x:1)', 7),
    ('(split horizontal:0.5:x)', 7),
    ('(split horizontal:0.5:-1)', 7),
    ('(split horizontal:0.5:2)', 7),
    ('(split horizontal:0.3)', 7),
    ('(split horizontal:0.3:0:0)', 7),
    ('(split  horizonta:0.5:0 )', 8),
    ('(clients max )', 9),
    ('(clients max:0:0 )', 9),
    ('(split horizontal:0.5:0 x)', 24),
    ('(split horizontal:0.5:0 (split horizontal:0.5:1', 47),
    ('(split horizontal:0.5:0 (split horizontal:0.5:1 ', 48),
    ('(split horizontal:0.5:0 (split horizontal:0.5:1 )', 49),
    ('(split horizontal:0.5:0 (split horizontal:0.5:1 )))', 50),
    ('(split horizontal:0.5:0 (clients max:1', 38),
])
def test_syntax_errors_position(hlwm, invalid_layout, error_pos):
    c = hlwm.call_xfail(['load', invalid_layout])
    m = re.match(r'^load: Syntax error at ([0-9]*): ', c.stderr)
    assert m is not None
    assert int(m.group(1)) == error_pos


@pytest.mark.parametrize("layout", [
    "(clients max:0)",
    " (  clients   vertical:0  )",
    "(split horizontal:0.3:0)",
    "(split vertical:0.3:0 (clients horizontal:0))",
])
def test_valid_layout_syntax(hlwm, layout):
    hlwm.call(['load', layout])

