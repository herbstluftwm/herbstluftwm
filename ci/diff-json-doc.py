#!/usr/bin/env python3

import subprocess
import os
import sys
import argparse
import json
import difflib


class GitDir:
    def __init__(self, dirpath):
        self.dirpath = dirpath

    def run(self, *cmd):
        """
        run a git command in the git repository in the dir git_tmp.dir
        """
        tmp_dir = self.dirpath
        full_cmd = [
            'git',
            '--git-dir=' + os.path.join(tmp_dir, '.git'),
            '--work-tree=' + tmp_dir
        ] + list(cmd)
        print(':: ' + ' '.join(full_cmd), file=sys.stderr)
        subprocess.run(full_cmd)


def parse_pr_id(text):
    """replace pr IDs by git refs and
    leave other ref identifiers unchanged
    """
    if text[0:1] == '#':
        return 'github/pull/' + text[1:] + '/head'
    else:
        return text


def get_json_doc(tmp_dir):
    return subprocess.run(['doc/gendoc.py', '--json'],
                          stdout=subprocess.PIPE,
                          universal_newlines=True,
                          cwd=tmp_dir).stdout


def run_pipe_stdout(cmd):
    return subprocess.run(cmd,
                          stdout=subprocess.PIPE,
                          universal_newlines=True).stdout


def rest_call(query, path, body=None):
    """query is GET/POST/PATCH"""
    github_token = os.environ['GITHUB_TOKEN']
    curl = ['curl', '-H', 'Authorization: token ' + github_token]
    curl += ['-X', query]
    if body is not None:
        curl += ['-d', json.dumps(body)]
    curl += ["https://api.github.com/repos/herbstluftwm/herbstluftwm/" + path]
    json_src = run_pipe_stdout(curl)
    return json.loads(json_src)


def post_comment(pr_id, text):
    # list existing comments:
    comments = rest_call('GET', f'issues/{pr_id}/comments')
    comment_id = None
    for c in comments:
        if c['user']['login'] == 'herbstluftwm-bot':
            comment_id = c['id']
            break

    body = {'body': text}
    if comment_id is not None:
        print(f'Updating existing comment {comment_id} in #{pr_id}', file=sys.stderr)
        rest_call('PATCH', f'issues/comments/{comment_id}', body=body)
    else:
        print(f'Creating new comment in #{pr_id}', file=sys.stderr)
        rest_call('POST', f'issues/{pr_id}/comments', body=body)


def main():
    parser = argparse.ArgumentParser(description='Show diffs in json doc between to refs')
    parser.add_argument('oldref',
                        help='the old version, e.g. github/master',
                        default='github/master',
                        nargs='?')
    parser.add_argument('newref', help='the new version, e.g. a pull request number like #1021')
    parser.add_argument('--collapse-diff-lines', default=100, type=int,
                        help='from which diff size on the diff is collapsed per default')
    parser.add_argument('--post-comment', default=None,
                        help='post diff as a comment in the specified ID of a github issue'
                             + ' Requires that the environment variable GITHUB_TOKEN is set')
    parser.add_argument('--post-comment-auto', action='store_const', default=False, const=True,
                        help='Automatically derive the argument to --post-comment from newref')
    args = parser.parse_args()
    comment_target = None
    if args.post_comment_auto:
        if args.newref[0:1] == '#':
            print("Deriving --post-comment={}".format(args.newref[1:]), file=sys.stderr)
            comment_target = args.newref[1:]
        else:
            msg_format = "Unable to derive github issue number from {}"
            msg_format += " (expecting the prefix '#')"
            print(msg_format.format(args.newref), file=sys.stderr)
            sys.exit(1)
    else:
        comment_target = args.post_comment

    git_root = run_pipe_stdout(['git', 'rev-parse', '--show-toplevel']).rstrip()
    tmp_dir = os.path.join(git_root, '.hlwm-tmp-diff-json')
    git_tmp = GitDir(tmp_dir)

    if not os.path.isdir(tmp_dir):
        subprocess.call(['git', 'clone', git_root, tmp_dir])

    # fetch all pull request heads
    git_tmp.run(
        'fetch',
        'https://github.com/herbstluftwm/herbstluftwm',
        '+refs/pull/*:refs/remotes/github/pull/*',
        '+master:github/master')

    oldref = parse_pr_id(args.oldref)
    newref = parse_pr_id(args.newref)
    print(f'Diffing »{oldref}« and »{newref}«', file=sys.stderr)
    print(f'Checking out {oldref}', file=sys.stderr)
    git_tmp.run('checkout', '-f', oldref)
    oldjson = get_json_doc(tmp_dir).splitlines(keepends=True)
    print(f'Checking out {newref}', file=sys.stderr)
    git_tmp.run('-c', 'advice.detachedHead=false', 'checkout', '-f', newref)
    newjson = get_json_doc(tmp_dir).splitlines(keepends=True)

    diff = list(difflib.unified_diff(oldjson, newjson, fromfile=args.oldref, tofile=args.newref))
    if comment_target is not None:
        gendoc_url = '/herbstluftwm/herbstluftwm/blob/master/doc/gendoc.py'
        comment = [f'Diff of the output of [`doc/gendoc.py --json`]({gendoc_url}):']
        details_attr = ' open' if len(diff) < args.collapse_diff_lines else ''
        comment += [f'<details{details_attr}>']
        comment += [f'<summary>Full diff ({len(diff)} lines)</summary>']
        comment += ['']
        comment += ['```diff']
        comment += [line.rstrip('\n') for line in diff]
        comment += ['```']
        comment += ['</details>']
        comment_txt = '\n'.join(comment)
        post_comment(comment_target.lstrip('#'), comment_txt)
    else:
        print("")
        sys.stdout.writelines(diff)


main()
