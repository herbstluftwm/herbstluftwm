#!/usr/bin/env python3
import json
import argparse
import textwrap

def printdoc_for_class(clsname, jsondoc, clsname2anchor={}):
    """print the documentation for a given class. However,
    if the documentation for it has already been generated,
    only insert a link to ot using clsname2anchor
    """
    if clsname in clsname2anchor:
        anchor, label = clsname2anchor[clsname]
        print(f'See <a href="#{anchor}">{label}</a>')
        return

    # otherwise, create anchor and label:
    anchor = 'doc_cls_' + clsname
    label = clsname + ' doc'
    clsname2anchor[clsname] = (anchor, label)

    objdoc = jsondoc['objects'][clsname]
    if 'doc' in objdoc:
        print(objdoc['doc'])
    print(f'<ul><a name="{anchor}"></a>')
    for _, attr in objdoc['attributes'].items():
        if attr['default_value'] is not None:
            default_val = '= ' + attr['default_value']
        else:
            default_val = ''
        if attr.get('doc', None) is not None:
            docstr = attr.get('doc', None)
        else:
            docstr = ''
        print(f"""
            <li>
             {attr['type']}
             {attr['name']}
             {default_val}
             {docstr}
            </li>
        """)
    for _, child in objdoc['children'].items():
        docstr = ': ' + child['doc'] if 'doc' in child else ''
        class_doc = jsondoc['objects'][child['type']].get('doc', '')
        print(f"""
        <li>
        <details>
        <summary> {child['name']} {docstr} </summary>

        <br/>
        """)
        printdoc_for_class(child['type'], jsondoc, clsname2anchor)
        print('</details></li>')

    print('</ul>')


def main():
    parser = argparse.ArgumentParser(description='Generate object documentation')
    parser.add_argument('jsondoc', help='the hlwm-doc.json file')

    args = parser.parse_args()

    with open(args.jsondoc, 'r') as fh:
        jsondoc = json.load(fh)

    print(textwrap.dedent("""
    The state of herbstluftwm can interactively be introspected
    and modified via the object system. Similarly to a file system,
    the objects are organized in a tree:
    """))

    printdoc_for_class('Root', jsondoc)

main()
