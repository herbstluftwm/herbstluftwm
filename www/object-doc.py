#!/usr/bin/env python3
import json
import argparse
import textwrap

def count_whitespace_prefix(string):
    """return the number of spaces at the beginning of the given string"""
    for i, ch in enumerate(string):
        if ch != ' ':
            return i
    return len(string)

def multiline_for_bulletitem(src):
    """requote a multiline asciidoc doc such
    that it can be put in the item of a bullet list"""
    lines = src.splitlines()
    lastline = ''
    # add explicit markers for code blocks
    newlines = []
    def force_linebreak():
        """insert a linebreak in `newlines`"""
        if len(newlines) > 0 and newlines[-1] == '+':
            # never add two '+' next to each other
            pass
        else:
            newlines.append('+')

    codeblock_indent = 0
    for l in lines:
        if l.startswith('  ') and not lastline.startswith('  '):
            force_linebreak()
            newlines += ['----']  # codeblock starts with 'l'
            codeblock_indent = count_whitespace_prefix(l)
        if not l.startswith('  ') and lastline.startswith('  '):
            # codeblock ended before 'l'
            # so dedent the lines of this codeblock by `codeblock_indent`
            for i, codeblockline in reversed(list(enumerate(newlines))):
                if not codeblockline.startswith('  '):
                    break
                newlines[i] = codeblockline[codeblock_indent:]
            # end codeblock:
            newlines += ['----']
            force_linebreak()
        else:
            codeblock_indent = min(codeblock_indent, count_whitespace_prefix(l))
        if l == '':
            force_linebreak()
        else:
            newlines.append(l)
        lastline = l
    return '\n'.join(newlines)


def printdoc_for_class(clsname, jsondoc, clsname2anchor={}, depth=0):
    """print the documentation for a given class. However,
    if the documentation for it has already been generated,
    only insert a link to ot using clsname2anchor
    """
    if clsname in clsname2anchor:
        anchor, label = clsname2anchor[clsname]
        print(f'<<{anchor},{label}>>')
        return

    # otherwise, create anchor and label:
    anchor = 'doc_cls_' + clsname
    label = '+' + clsname + '+ doc'
    clsname2anchor[clsname] = (anchor, label)
    ws_prefix = depth * ' ' + '   '  # whitespace prefix

    objdoc = jsondoc['objects'][clsname]
    print(f'[[{anchor}]]')
    if 'doc' in objdoc:
        print(multiline_for_bulletitem(objdoc['doc']))
    print('')
    bulletprefix = depth * ' ' + depth * '*'
    for _, attr in objdoc['attributes'].items():
        if attr['default_value'] is not None:
            default_val = '= ' + attr['default_value']
        else:
            default_val = ''
        if attr.get('doc', None) is not None:
            docstr = attr.get('doc', None)
        else:
            docstr = ''
        print(f"{ws_prefix}{bulletprefix}* {attr['type']} +{attr['name']}+ {default_val} {docstr}")
    for _, child in objdoc['children'].items():
        docstr = ': ' + child['doc'].strip() if 'doc' in child else ''
        class_doc = jsondoc['objects'][child['type']].get('doc', '')
        print(f"{ws_prefix}{bulletprefix}* +{child['name']}+ {docstr}", end='')
        printdoc_for_class(child['type'], jsondoc, clsname2anchor, depth=depth + 1)


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
