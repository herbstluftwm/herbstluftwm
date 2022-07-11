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
        '* { border-style: invalidstyle; }': 'Expected \"solid\"',
    }
    for source, error in input2error.items():
        assert hlwm.call_xfail(['debug-css', '--print-css', source]) \
            .expect_stderr(error)


def test_css_basic_selectors(hlwm):
    tree = '(client-decoration (focus (tab urgent)) (normal))'
    selector2match = {
        '.client-decoration': [''],
        'client-decoration': [],  # the . is missing
        '#client-decoration': [],  # wrong access
        '.client-decoration>.focus': ['0'],
        '.client-decoration > .focus': ['0'],
        '.client-decoration  >  .focus': ['0'],
        '.client-decoration >.focus': ['0'],
        '.client-decoration * > .focus': [],
        '.client-decoration > .focus + *': ['1'],
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


def test_css_custom_name(hlwm):
    tree = """
        (client-decoration
           (something with index0)
           (another with custom))
    """
    selector2match = {
        '.something': ['0'],
        '.with': ['0', '1'],
        '.custom': ['1'],
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
        (client-decoration
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
        '.client-decoration * + .tab': ['1 1', '1 2', '2', '1'],
        '.client-decoration > * + .tab': ['1', '2'],
    }
    for selector, expected in selector2match.items():
        cmd = [
            'debug-css', '--tree=' + tree,
            '--query-tree-indices=' + selector,
            ''  # empty css
        ]
        output = hlwm.call(cmd).stdout.splitlines()
        assert sorted(output) == ['match: ' + x for x in sorted(expected)]


def test_css_property_applier(hlwm):
    decl2computed = {
        'border: 2px solid #9fbc00':
        '\n'.join([
            f'border-{side}-{prop}: {value};'
            for side in ['top', 'left', 'right', 'bottom']
            # 'solid' isn't shown because it's the default style
            for prop, value in [('width', '2px'), ('color', '#9fbc00')]]),
        'border-top: 5px solid #ffddee':
        """\
        border-top-width: 5px;
        border-top-color: #ffddee;
        """,
        'border-left: 5px solid #ffddee':
        """\
        border-left-width: 5px;
        border-left-color: #ffddee;
        """,
        'border-right: 5px solid #ffddee':
        """\
        border-right-width: 5px;
        border-right-color: #ffddee;
        """,
        'border-bottom: 5px solid #ffddee':
        """\
        border-bottom-width: 5px;
        border-bottom-color: #ffddee;
        """,
        'border-width: 4px 2px':
        """\
        border-top-width: 4px;
        border-bottom-width: 4px;
        border-left-width: 2px;
        border-right-width: 2px;
        """,
        'border-width: 2px 3px 4px 5px':
        """\
        border-top-width: 2px;
        border-right-width: 3px;
        border-bottom-width: 4px;
        border-left-width: 5px;
        """,
        'display: flex': '',  # flex is the default
    }
    simple_props = [
        'min-height: 5px',
        'min-width: 6px',
        'display: none',
        'color: #125323',
        'text-align: center',
        'background-color: #fb4ace',
    ]
    for prop in simple_props:
        decl2computed[prop] = prop
    for css_decl, computed_style in decl2computed.items():
        css_decl = css_decl.rstrip(' \n;') + ';'
        css = f"""
        .testclass {{
            {css_decl}
        }}
        """
        tree = '((a) (testclass) (b))'
        cmd = [
            'debug-css', '--tree=' + tree,
            '--compute-style=1',
            css
        ]

        def normalize(buf):
            return sorted([line.strip().rstrip(';') for line in buf.splitlines()])

        expected = normalize(textwrap.dedent(computed_style))
        output = normalize(hlwm.call(cmd).stdout)
        assert expected == output


def test_css_computed_style(hlwm):
    tree = """
        (client-decoration
           (some-buttons in the future maybe)
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

    .some-buttons.future {
        margin-left: 5px;
    }

    .some-buttons {
        margin-left: 3px;
        margin-right: 2px;
    }

    .the.future {
        border-width: 0px;
    }
    """
    index2style = {
        '1 0':  # the active tab
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
        '1 1':  # the tab next to it has a thinner left border
        """\
        border-top-width: 2px;
        border-right-width: 4px;
        border-bottom-width: 6px;
        border-left-width: 1px;
        """,
        '0':  # the some-buttons...
        """\
        margin-left: 5px;
        margin-right: 2px;
        """
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
