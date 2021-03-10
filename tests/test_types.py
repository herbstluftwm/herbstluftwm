def test_color_rgb(hlwm):
    hlwm.attr.theme.color = '#9fbc00'
    assert hlwm.attr.theme.color() == '#9fbc00'


def test_color_rgba(hlwm):
    hlwm.attr.theme.color = '#9fbc0043'
    assert hlwm.attr.theme.color() == '#9fbc0043'

    hlwm.attr.theme.color = '#9fbc12ab'
    assert hlwm.attr.theme.color() == '#9fbc12ab'
    hlwm.call('compare theme.color = #9fbc12ab')
    hlwm.call('compare theme.color != #9fbc12ba')
    hlwm.call('compare theme.color != #9fbc13ab')

    hlwm.attr.theme.color = '#9fbc3300'
    assert hlwm.attr.theme.color() == '#9fbc3300'
    hlwm.call('compare theme.color = #9fbc3300')
    hlwm.call('compare theme.color != #9fbc33')

    hlwm.attr.theme.color = '#9fbc00ff'
    # no transparency is simplified to a normal rgb value:
    assert hlwm.attr.theme.color() == '#9fbc00'
    hlwm.call('compare theme.color = #9fbc00')
    hlwm.call('compare theme.color = #9fbc00ff')
    hlwm.call('compare theme.color != #9fbc00f0')


def test_color_invalid(hlwm):
    hlwm.call_xfail('set_attr theme.color #9xbc00') \
        .expect_stderr('cannot allocate color')

    hlwm.call_xfail('set_attr theme.color lilablassblau') \
        .expect_stderr('cannot allocate color')

    hlwm.call_xfail('set_attr theme.color #9fbc000') \
        .expect_stderr('cannot allocate color')

    hlwm.call_xfail('set_attr theme.color #9fbc00x') \
        .expect_stderr('cannot allocate color')

    hlwm.call_xfail('set_attr theme.color #9fbc00fg') \
        .expect_stderr('invalid alpha')

    hlwm.call_xfail('set_attr theme.color #9fbc00-3') \
        .expect_stderr('invalid alpha')


def test_color_names(hlwm):
    colors = [('red', '#ff0000'), ('green', '#00ff00'), ('blue', '#0000ff')]
    for name, rgb in colors:
        hlwm.attr.theme.color = name
        assert hlwm.attr.theme.color() == rgb


def test_int_uint_unparsable_suffix(hlwm):
    for attrtype in ['int', 'uint']:
        attr = 'my_' + attrtype
        hlwm.call(['new_attr', attrtype, attr])

        hlwm.call_xfail(['set_attr', attr, '3f']) \
            .expect_stderr('unparsable suffix: f')

        hlwm.call_xfail(['set_attr', attr, '3+']) \
            .expect_stderr('unparsable suffix')

        hlwm.call_xfail(['set_attr', attr, '3.0']) \
            .expect_stderr('unparsable suffix')

        hlwm.call_xfail(['set_attr', attr, '+3x']) \
            .expect_stderr('unparsable suffix')


def test_type_children_names(hlwm):
    types = hlwm.list_children('types')
    for t in types:
        assert hlwm.attr.types[t].fullname() == t
