import json
import pytest
import conftest
import re


@pytest.fixture()
def json_doc():
    json_filepath = conftest.BINDIR / "doc" / "hlwm-doc.json"
    with open(json_filepath, 'r') as fh:
        doc = json.loads(fh.read())
    return doc


def create_client(hlwm):
    winid, _ = hlwm.create_client()
    return f'clients.{winid}'


def create_clients_with_all_links(hlwm):
    # enforce that 'clients.focus' exists
    winid, _ = hlwm.create_client()
    # enforce that 'clients.dragged' exists
    hlwm.call('floating on')
    hlwm.call('drag "" move')
    return 'clients'


def create_frame_split(hlwm):
    hlwm.call('split explode')
    return 'tags.0.tiling.root'


def create_tag_with_all_links(hlwm):
    """create a tag with focused_client set"""
    hlwm.create_client()
    return 'tags.0'


def create_panel(hlwm):
    """
    Connect to the display; we only need x11 here, so
    let us not introduce it as a dependency for the all
    test cases but instead connect to the X display manually.

    Unfortunately, we never have the chance to properly close
    the display, but I hope it's OK for this single test.
    """
    display = conftest.xlib_connect_to_display(hlwm.env['DISPLAY'])
    x11 = conftest.X11(display)
    _, winid = x11.create_client(geometry=(0, 0, 800, 30),
                                 window_type='_NET_WM_WINDOW_TYPE_DOCK')
    # write the x11 bridge to a variable with a long life-span to avoid
    # that the garbage-collection closes it (this would close the panel).
    create_panel.x11 = x11
    return f'panels.{winid}'


# map every c++ class name to a function ("constructor") accepting an hlwm
# fixture and returning the path to an example object of the C++ class
classname2examplepath = [
    ('ByName', lambda _: 'monitors.by-name'),
    ('Client', create_client),
    ('ClientManager', create_clients_with_all_links),
    ('DecTriple', lambda _: 'theme.tiling'),
    ('DecorationScheme', lambda _: 'theme.tiling.urgent'),
    ('FrameLeaf', lambda _: 'tags.0.tiling.root'),
    ('FrameSplit', create_frame_split),
    ('HSTag', create_tag_with_all_links),
    ('Monitor', lambda _: 'monitors.0'),
    ('MonitorManager', lambda _: 'monitors'),
    ('Panel', create_panel),
    ('Root', lambda _: ''),
    ('Settings', lambda _: 'settings'),
    ('TagManager', lambda _: 'tags'),
    ('Theme', lambda _: 'theme'),
    ('TypesDoc', lambda _: 'types'),
]


@pytest.mark.parametrize('clsname,object_path', classname2examplepath)
def test_documented_attributes_writable(hlwm, clsname, object_path, json_doc):
    """test whether the writable field is correct. This checks the
    existence of the attributes implicitly
    """
    object_path = object_path(hlwm)
    for _, attr in json_doc['objects'][clsname]['attributes'].items():
        print("checking attribute {}::{}".format(clsname, attr['cpp_name']))
        full_attr_path = '{}.{}'.format(object_path, attr['name']).lstrip('.')
        value = hlwm.get_attr(full_attr_path)
        if value == 'default':
            continue
        if attr['writable']:
            hlwm.call(['set_attr', full_attr_path, value])
        else:
            hlwm.call_xfail(['set_attr', full_attr_path, value]) \
                .expect_stderr('attribute is read-only')


def types_and_shorthands():
    """a mapping from type names in the json doc to their
    one letter short hands in the output of 'attr'
    """
    return {
        'int': 'i',
        'uint': 'u',
        'bool': 'b',
        'decimal': 'd',
        'color': 'c',
        'string': 's',
        'regex': 'r',
        'SmartFrameSurroundings': 'n',
        'SmartWindowSurroundings': 'n',
        'ShowFrameDecorations': 'n',
        'SplitAlign': 'n',
        'LayoutAlgorithm': 'n',
        'TagSelectionStrategy': 'n',
        'TextAlign': 'n',
        'TitleWhen': 'n',
        'font': 'f',
        'Rectangle': 'R',
        'WindowID': 'w',
        'MaybeColor': 's',
        'MaybeULong': 's',
        'MaybeInt': 's',
        'CssSource': 's',
        'CssFile::Ptr': 's',
    }


@pytest.mark.parametrize('clsname,object_path', classname2examplepath)
def test_documented_attribute_type(hlwm, clsname, object_path, json_doc):
    object_path = object_path(hlwm)
    attr_output = hlwm.call(['attr', object_path]).stdout.splitlines()
    attr_output = [line.split(' ') for line in attr_output if '=' in line]
    attrname2shorttype = {line[4]: line[1] for line in attr_output}
    fulltype2shorttype = types_and_shorthands()
    for _, attr in json_doc['objects'][clsname]['attributes'].items():
        assert fulltype2shorttype[attr['type']] == attrname2shorttype[attr['name']]


@pytest.mark.parametrize('clsname,object_path', classname2examplepath)
def test_documented_children_exist(hlwm, clsname, object_path, json_doc):
    object_path = object_path(hlwm)
    object_path_dot = object_path + '.' if object_path != '' else ''
    for _, child in json_doc['objects'][clsname]['children'].items():
        if child['name'] is not None:
            hlwm.call(['object_tree', object_path_dot + child['name']])
        else:
            assert child['name_patern'] is not None


@pytest.mark.parametrize('clsname,object_path', classname2examplepath)
def test_attributes_and_children_are_documented(hlwm, clsname, object_path, json_doc):
    # if a path matches the following re, then it's OK if it
    # is not mentioned explicitly in the docs
    undocumented_paths = '|'.join([
        r'tags\.[0-9]+',
        r'clients\.0x[0-9a-f]+',
        r'monitors\.[0-9]+',
        r'tags\.by-name\.default',
    ])
    undocumented_path_re = re.compile(r'^({})[\. ]*$'.format(undocumented_paths))

    object_path = object_path(hlwm)
    object_path_dot = object_path + '.' if object_path != '' else ''

    entries = hlwm.complete(['get_attr', object_path_dot], position=1, partial=True)
    for full_entry_path in entries:
        if undocumented_path_re.match(full_entry_path):
            continue
        assert full_entry_path[0:len(object_path_dot)] == object_path_dot
        entry = full_entry_path[len(object_path_dot):]
        if entry[-1] == '.':
            assert entry[0:-1] in json_doc['objects'][clsname]['children']
        else:
            assert entry[-1] == ' ', "it's an attribute if it's no child"
            assert entry[0:-1] in json_doc['objects'][clsname]['attributes']


@pytest.mark.parametrize('clsname,object_path', classname2examplepath)
def test_class_doc(hlwm, clsname, object_path, json_doc):
    path = object_path(hlwm)
    attr_output = hlwm.call(['attr', path]).stdout

    object_doc = json_doc['objects'][clsname].get('doc', None)
    if object_doc is not None:
        assert attr_output.startswith(object_doc)
    else:
        # if no class doc is in the json file, then there
        # is indeed none:
        assert re.match(r'1 child:|[0-9]* children[\.:]$', attr_output.splitlines()[0])


@pytest.mark.parametrize('clsname,object_path', classname2examplepath)
def test_help_on_attribute_vs_json(hlwm, clsname, object_path, json_doc):
    path = object_path(hlwm)
    attrs_doc = json_doc['objects'][clsname]['attributes']
    for _, attr in attrs_doc.items():
        attr_name = attr['name']
        help_txt = hlwm.call(['help', f'{path}.{attr_name}'.lstrip('.')]).stdout

        assert f"Attribute '{attr_name}'" in help_txt
        doc = attr.get('doc', '')
        assert doc in help_txt


@pytest.mark.parametrize('clsname,object_path', classname2examplepath)
def test_help_on_children_vs_json(hlwm, clsname, object_path, json_doc):
    path = object_path(hlwm)
    child_doc = json_doc['objects'][clsname]['children']
    for _, child in child_doc.items():
        name = child['name']
        help_txt = hlwm.call(['help', f'{path}.{name}'.lstrip('.')]).stdout

        if 'doc' in child:
            assert f"Entry '{name}'" in help_txt
            assert child['doc'] in help_txt
        else:
            # if there is no child doc in the json
            # then there should be one in the cpp source
            # and thus also not in the help output:
            assert f"Entry '{name}'" not in help_txt


@pytest.mark.parametrize('clsname,object_path', classname2examplepath)
def test_attribute_doc_in_json(hlwm, clsname, object_path, json_doc):
    """
    Verify that if an attribute has a doc string (according to the 'help' command)
    then the doc string is also present in the json.
    """
    path = object_path(hlwm)

    for _, attribute in json_doc['objects'][clsname]['attributes'].items():
        name = attribute['name']
        help_txt = hlwm.call(['help', f'{path}.{name}'.lstrip('.')]).stdout
        help_lines = help_txt.rstrip().split('\n')
        doc_in_help = ''
        # the doc is everything after 'Current value: ..'
        for line_idx in range(0, len(help_lines) - 1):
            # a line starting with 'Current value: '
            found = help_lines[line_idx].startswith('Current value: ')
            # and the next line is empty:
            found = found and help_lines[line_idx + 1] == ''
            if found:
                # the doc is everything after the empty line:
                doc_in_help = '\n'.join(help_lines[line_idx + 2:])
                break
        if not doc_in_help.startswith('Current value:'):
            # if there is a doc printed by 'help', then it
            # should also be present in the json:
            assert doc_in_help == attribute.get('doc', '').rstrip()
