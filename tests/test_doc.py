import json
import pytest
import conftest
import os
import re


@pytest.fixture()
def json_doc():
    json_filepath = os.path.join(conftest.BINDIR, 'doc/hlwm-doc.json')
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
    ('HSTag', lambda _: 'tags.0'),
    ('Monitor', lambda _: 'monitors.0'),
    ('MonitorManager', lambda _: 'monitors'),
    ('Root', lambda _: ''),
    ('Settings', lambda _: 'settings'),
    ('TagManager', lambda _: 'tags'),
    ('Theme', lambda _: 'theme'),
]


@pytest.mark.parametrize('clsname,object_path', classname2examplepath)
def test_documented_attributes_exist(hlwm, clsname, object_path, json_doc):
    object_path = object_path(hlwm)
    for _, attr in json_doc['objects'][clsname]['attributes'].items():
        print("checking attribute {}::{}".format(clsname, attr['cpp_name']))
        hlwm.get_attr('{}.{}'.format(object_path, attr['name']).lstrip('.'))


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
        'SplitAlign': 'n',
        'LayoutAlgorithm': 'n',
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
        hlwm.call(['object_tree', object_path_dot + child['name']])


@pytest.mark.parametrize('clsname,object_path', classname2examplepath)
def test_attributes_and_children_are_documented(hlwm, clsname, object_path, json_doc):
    # if a path matches the following re, then it's OK if it
    # is not mentioned explicitly in the docs
    undocumented_paths = '|'.join([
        r'tags\.[0-9]+',
        r'clients\.0x[0-9a-f]+',
        r'monitors\.[0-9]+',
        r'tags\.by-name\.default',
        # TODO: the theme children do not yet use the Child_<...> pattern
        r'theme\.(|.*\.)(active|normal|urgent)',
        r'theme\.(fullscreen|tiling|floating|minimal)',
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
