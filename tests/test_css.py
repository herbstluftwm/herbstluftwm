import textwrap


def test_basic_css_normalize(hlwm):
    input2normalize = {
        "// comment\n foo .p, bar.c,* +c { border-width: 4px 2px; }": """\
        foo .p ,
        bar.c ,
        * +c {
            border-width: 4px 2px;
        }
        """,
        "x { /*com\nment*/ border-width: 1px 1px 1px 1px}": """\
        x {
            border-width: 1px 1px 1px 1px;
        }
        """,
        "a.b { border-width: 2px } c.d e{border-width: 5px}": """\
        a.b {
            border-width: 2px;
        }

        c.d e {
            border-width: 5px;
        }
        """,
        "* { }": """\
        * {
        }
        """,
        ".cl { border-width: 1px; border-width: 1px; border-width: 1px}":
        """\
        .cl {
            border-width: 1px;
            border-width: 1px;
            border-width: 1px;
        }
        """,
        "foo.class1     .class2    {border-left-width:4px;   border-right-width   : 2px  ;   }   //": """\
        foo.class1 .class2 {
            border-left-width: 4px;
            border-right-width: 2px;
        }
        """,
        "// foo": "",
        "// foo\n": "",
        "/* end */": "",
    }

    for source, normalized in input2normalize.items():
        output = hlwm.call(['debug-css', '--print', source]).stdout
        assert output == textwrap.dedent(normalized)
        # check that pretty printing is idempotent:
        assert hlwm.call(['debug-css', '--print', output]).stdout == output


def test_basic_css_parse_error(hlwm):
    input2error = {
        '{ border-width: 2px; }': "need at least one selector",
        ', { border-width: 2px; }': "selector must not be empty",
        'p { border-width: 2px;': "Expected }",
        'p } border-width: 2px;': "Expected { but got \"}",
        '/* unmatched': r'Expected \*/ but got EOF',
        '/* unmatched\n': r'Expected \*/ but got EOF',
        '/*\n': r'Expected \*/ but got EOF',
    }
    for source, error in input2error.items():
        assert hlwm.call_xfail(['debug-css', '--print', source]) \
            .expect_stderr(error)
