#!/usr/bin/env python3

import argparse
import os
import sys
import re


def findfiles(sourcedir, regex_object):
    """find all files in the given 'sourcedir' whose
    filename matches 'regex_object'
    """
    for root, dirs, files in os.walk(sourcedir):
        for file in files:
            if regex_object.match(file):
                 yield os.path.join(root, file)


def extract_file_tokens(filepath):
    # in the following, it's important to use
    # non-capturing groups (?: .... )
    token_types = [
        '#[a-zA-Z][^\n]*\n',  # preprocessor
        '//[^\n]*\n',  # single-line comment
        '/\*(?:[^\*]*|\**[^/*])*\*/',  # multiline comment
        '[a-zA-Z_][a-zA-Z0-9_]*',  # identifiers
        '\'(?:\\\'|[^\']*)\'',
        "\"(?:\\\"|[^\"]*)\"",
        "[-+<>/*]",  # operators
        "[\(\),;:{}\?|~]",
        '[\t ][\t ]*',
        '\n',
    ]
    entire_regex = ['(?:{})'.format(t) for t in token_types]
    entire_regex = re.compile('(' + '|'.join(entire_regex) + ')')
    with open(filepath, 'r') as fh:
        for t in entire_regex.split(fh.read()):
            if t is not None and t.strip(' \t') != '':
                yield t


def pretty_print_token_list(tokens, indent='  '):
    """print all the tokens and auto-indent based on { }"""
    number_nested_braces = 0
    first_in_line = True
    line_number = 1
    for tok in tokens:
        if tok == '}':
            number_nested_braces -= 1
        linecontent = tok.rstrip('\n')
        if linecontent != '':
            if first_in_line:
                print('{:4}: '.format(line_number) + number_nested_braces * indent, end='')
            print(' \'{}\''.format(linecontent), end='')
        for ch in tok:
            if ch == '\n':
                line_number += 1
        if tok.endswith('\n'):
            print('')
            first_in_line = True
        else:
            first_in_line = False
        if tok == '{':
            number_nested_braces += 1
    print()


class TokenTree:
    """a TokenTree is one of the following:
        - a literate token
        - a token group that is enclosed by ( ), [ ], or { } tokens
    """
    def __init__(self):
        """this should not be called directly"""
        # literate token:
        self.literate = None
        # token group
        self.opening_token = None
        self.enclosed_tokens = []  # list of TokenTree objects
        self.closing_token = None
        pass

    @staticmethod
    def LiterateToken(tok):
        tt = TokenTree()
        tt.literate = tok
        return tt

    @staticmethod
    def TokenGroup(opening, enclosed, closing):
        tt = TokenTree()
        tt.opening_token = opening
        tt.enclosed_tokens = enclosed
        tt.closing_token = closing
        return tt

    def PrettyPrintList(tokentree_list, curindent=''):
        for t in tokentree_list:
            if t.literate is not None:
                if t.literate.strip() == '':
                    continue
                print(curindent + t.literate)
            else:
                print(curindent + t.opening_token)
                TokenTree.PrettyPrintList(t.enclosed_tokens, curindent=curindent + '  ')
                print(curindent + t.closing_token)


class TokenStream:
    def __init__(self, tokens):
        self.tokens = tokens
        self.pos = 0

    def empty(self):
        return self.pos >= len(self.tokens)

    def pop(self):
        t =  self.tokens[self.pos]
        self.pos += 1
        #print("yielding token '{}'".format(t))
        return t

    def undo_pop(self):
        """undo the last pop operation"""
        self.pos -= 1


def build_token_tree_list(token_stream):
    """return a list of TokenTree objects"""
    while not token_stream.empty():
        t = token_stream.pop()
        if t in [')', '}', ']']:
            token_stream.undo_pop()
            break
        elif t in ['(', '{', '[']:
            nested = list(build_token_tree_list(token_stream))
            closing = token_stream.pop()
            #assert closing in [')', '}', ']']
            yield TokenTree.TokenGroup(t, nested, closing)
        else:
            yield TokenTree.LiterateToken(t)


def main():
    parser = argparse.ArgumentParser(description='extract hlwm doc from the source code')
    parser.add_argument('--sourcedir', default='./src/',
                        help='directory containing the source files')
    parser.add_argument('--fileregex', default='.*\.(h|cpp)$',
                        help='consider files whose name matches this regex')
    parser.add_argument('--tokenize-single-file',
                        help='tokenize a particular file and then exit')
    parser.add_argument('--tokentree-single-file',
                        help='print the token tree of a particular file and then exit')
    args = parser.parse_args()
    files = findfiles(args.sourcedir, re.compile(args.fileregex))

    if args.tokenize_single_file is not None:
        tokens = extract_file_tokens(args.tokenize_single_file)
        pretty_print_token_list(tokens)
        return 0

    if args.tokentree_single_file is not None:
        tokens = extract_file_tokens(args.tokentree_single_file)
        stream = TokenStream(list(tokens))
        tt_list = list(build_token_tree_list(stream))
        if not stream.empty():
            print('Warning: unmatched [ ], ( ), { }', file=sys.stderr)
            print("Remaining tokens are:", file=sys.stderr)
            while not stream.empty():
                print("token: {}".format(stream.pop()), file=sys.stderr)
        TokenTree.PrettyPrintList(tt_list)
        return 0

EXITCODE = main()
if EXITCODE is not None:
    sys.exit(EXITCODE)
