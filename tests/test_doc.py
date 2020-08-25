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


def create_frame_split(hlwm):
    hlwm.call('split explode')
    return 'tags.0.tiling.root'


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


@pytest.mark.parametrize('clsname,object_path', [
    ('ByName', 'monitors.by-name'),
    ('Client', create_client),
    ('ClientManager', 'clients'),
    ('DecTriple', 'theme.tiling'),
    ('DecorationScheme', 'theme.tiling.urgent'),
    ('FrameLeaf', 'tags.0.tiling.root'),
    ('FrameSplit', create_frame_split),
    ('HSTag', 'tags.0'),
    ('Monitor', 'monitors.0'),
    ('MonitorManager', 'monitors'),
    ('Root', ''),
    ('Settings', 'settings'),
    ('TagManager', 'tags'),
    ('Theme', 'theme'),
])
def test_attributes(hlwm, clsname, object_path, json_doc):
    # if a path matches the following re, then it's OK if it
    # is not mentioned explicitly in the docs
    undocumented_paths = '|'.join([
        r'tags\.[0-9]+',
        r'monitors\.[0-9]+',
        r'tags\.by-name\.default',
        # the theme children do not yet use the Child_<...> pattern
        r'theme\.(|.*\.)(active|normal|urgent)',
        r'theme\.(fullscreen|tiling|floating|minimal)',
    ])
    undocumented_path_re = re.compile(r'^({})[\. ]*$'.format(undocumented_paths))

    object_doc = None
    for obj in json_doc['objects']:
        if obj['classname'] == clsname:
            object_doc = obj
            break
    assert object_doc is not None

    if not isinstance(object_path, str):
        # if it's not a string directly, it's a function returning
        # a string
        object_path = object_path(hlwm)

    # 1. test that all documented attributes actually exist and that it has
    # the right type
    attr_output = hlwm.call(['attr', object_path]).stdout.splitlines()
    attr_output = [l.split(' ') for l in attr_output if '=' in l]
    attrname2shorttype = dict([(l[4], l[1]) for l in attr_output])
    fulltype2shorttype = types_and_shorthands()
    for attr in object_doc['attributes']:
        print("checking attribute {}::{}".format(clsname, attr['cpp_name']))
        hlwm.get_attr('{}.{}'.format(object_path, attr['name']).lstrip('.'))
        assert fulltype2shorttype[attr['type']] == attrname2shorttype[attr['name']]

    # collect all attributes and children
    attributes = dict([(a['name'], a) for a in object_doc['attributes']])
    children = dict([(c['name'], c) for c in object_doc['children']])

    # 2. test that all children/attributes are documented
    object_path_dot = object_path + '.' if object_path != '' else ''
    entries = hlwm.complete(['get_attr', object_path_dot], position=1, partial=True)
    for full_entry_path in entries:
        if undocumented_path_re.match(full_entry_path):
            continue
        assert full_entry_path[0:len(object_path_dot)] == object_path_dot
        entry = full_entry_path[len(object_path_dot):]
        if entry[-1] == '.':
            assert entry[0:-1] in children
        else:
            assert entry[0:-1] in attributes
