import textwrap


def test_basic_css_normalize(hlwm):
    input2normalize = {
        "// comment\n foo .p, bar.c,* +c { border-width: 4px 2px; }": """\
        foo .p ,
        bar.c ,
        * + c {
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
        ".cl { border-color: #9fbc00; background-color: #1234ab; }":
        """\
        .cl {
            border-color: #9fbc00;
            background-color: #1234ab;
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
        output = hlwm.call(['debug-css', '--print-css', source]).stdout
        assert output == textwrap.dedent(normalized)
        # check that pretty printing is idempotent:
        assert hlwm.call(['debug-css', '--print-css', output]).stdout == output


def test_basic_css_parse_error(hlwm):
    input2error = {
        '{ border-width: 2px; }': "need at least one selector",
        ', { border-width: 2px; }': "selector must not be empty",
        'p { border-width: 2px;': "Expected }",
        'p } border-width: 2px;': "Expected { but got \"}",
        '/* unmatched': r'Expected \*/ but got EOF',
        '/* unmatched\n': r'Expected \*/ but got EOF',
        '/*\n': r'Expected \*/ but got EOF',
        '* // { }': "but got EOF",
        '* { // }': "Expected } but got EOF",
    }
    for source, error in input2error.items():
        assert hlwm.call_xfail(['debug-css', '--print-css', source]) \
            .expect_stderr(error)


def test_basic_dummy_tree(hlwm):
    """testing the interface used for testing...."""
    input2normalize = {
        '()': '()',
        '(win (c focused (e f)))': '(win\n  (c focused\n    (e f)))',
        '(win (c (e f)focused))': '(win\n  (c focused\n    (e f)))',
        '((c d (e f)))': '(\n  (c d\n    (e f)))',
        '((() (e f)))': '(\n  (\n    ()\n    (e f)))',
    }
    for source, normalized in input2normalize.items():
        output = hlwm.call(['debug-css', '--print-tree', '--tree=' + source, '']).stdout
        assert output == normalized + '\n'
        # check that pretty printing is idempotent:
        assert hlwm.call(['debug-css', '--print-tree', '--tree=' + output, '']).stdout == output

    input2error = {
        '(': r'Expected \) but got EOF',
        '( (a ())))': r'Expected EOF but got.*\)',
        '() ()': r'Expected EOF but got.*\(',
    }
    for source, error in input2error.items():
        hlwm.call_xfail(['debug-css', '--print-tree', '--tree=' + source, '']) \
            .expect_stderr(error)


def test_css_property_parsing(hlwm):
    input2error = {
        '* { border-width: 1sdfpx; }': "unparsable suffix",
        '* { border-width: 1px 2px 3px 4px 5px; }': '"border-width" does not accept 5',
    }
    for source, error in input2error.items():
        assert hlwm.call_xfail(['debug-css', '--print-css', source]) \
            .expect_stderr(error)


def test_css_basic_selectors(hlwm):
    tree = '(window (focus (tab urgent)) (normal))'
    selector2match = {
        '.window': [''],
        'window': [],  # the . is missing
        '#window': [],  # wrong access
        '.window>.focus': ['0'],
        '.window > .focus': ['0'],
        '.window  >  .focus': ['0'],
        '.window >.focus': ['0'],
        '.window * > .focus': [],
        '.window > .focus + *': ['1'],
        ':first-child': ['0', '0 0'],
        ':last-child': ['1', '0 0'],
        '.focus :last-child': ['0 0'],
        '.focus:last-child': [],
        '.focus:first-child': ['0'],
        '* + .normal': ['1'],
        '*': ['', '0', '0 0', '1'],
        '* *': ['0', '0 0', '1'],
    }
    for selector, expected in selector2match.items():
        cmd = [
            'debug-css', '--tree=' + tree,
            '--query-tree-indices=' + selector,
            ''  # empty css
        ]
        output = hlwm.call(cmd).stdout.splitlines()
        assert sorted(output) == ['match: ' + x for x in sorted(expected)]


def test_css_sibling_cominbators(hlwm):
    tree = """
        (window
           (something with index0)
           (tabbar tab index1
                (tab)
                (tab focus)
                (tab urgent))
           (tab anything))
    """
    selector2match = {
        '.tab + .tab': ['1 1', '1 2', '2'],
        '* + .tab + .tab': ['1 2', '2'],
        '.tab + .tab + .tab': ['1 2'],
        '.tab .tab + .tab': ['1 1', '1 2'],
        '* + .tab': ['1 1', '1 2', '2', '1'],
        '.window * + .tab': ['1 1', '1 2', '2', '1'],
        '.window > * + .tab': ['1', '2'],
    }
    for selector, expected in selector2match.items():
        cmd = [
            'debug-css', '--tree=' + tree,
            '--query-tree-indices=' + selector,
            ''  # empty css
        ]
        output = hlwm.call(cmd).stdout.splitlines()
        assert sorted(output) == ['match: ' + x for x in sorted(expected)]


def test_css_computed_style(hlwm):
    tree = """
        (window
           (some buttons in the future maybe)
           (tabbar tab index1
                (tab focus)
                (tab)
                (tab urgent)
                (tab))
           (some more buttons at the end))
    """
    css = """
    .tab + .tab {
        border-left-width: 1px;
    }
    .tab.focus {
        border-color: #9fbc00;
    }

    /* this definition is later, but less specific */
    .tab {
        border-width: 2px 4px 6px 8px;
    }

    /* and so this is overwritten entirely */
    * {
        border-width: 77px;
    }
    """
    index2style = {
        '1 0': # the active tab
        """\
        border-top-color: #9fbc00;
        border-right-color: #9fbc00;
        border-bottom-color: #9fbc00;
        border-left-color: #9fbc00;
        border-top-width: 2px;
        border-right-width: 4px;
        border-bottom-width: 6px;
        border-left-width: 8px;
        """,
        '1 1': # the tab next to it has a thinner left border
        """\
        border-top-width: 2px;
        border-right-width: 4px;
        border-bottom-width: 6px;
        border-left-width: 1px;
        """,
    }
    for tree_index, computed_style in index2style.items():
        cmd = [
            'debug-css', '--tree=' + tree,
            '--compute-style=' + tree_index,
            css
        ]
        expected = sorted(textwrap.dedent(computed_style).strip().splitlines())
        output = sorted(hlwm.call(cmd).stdout.splitlines())
        assert expected == output


def test_debug_css_errors(hlwm):
    """test that the debug-css command itself does correct
    error handling"""
    hlwm.call_xfail('debug-css --compute-style=lkj ""') \
        .expect_stderr("stoi")
    hlwm.call_xfail('debug-css --compute-style=8 ""') \
        .expect_stderr("--compute-style requires a tree")
    hlwm.call_xfail('debug-css --tree="()" --compute-style=8 ""') \
        .expect_stderr("invalid tree index")
    hlwm.call_xfail('debug-css --tree="()" --compute-style="0 8" ""') \
        .expect_stderr("invalid tree index")
