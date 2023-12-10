# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import os
import pdb
from pprint import pprint
import re
import subprocess
import sys
import traceback
from pathlib import Path

# Replace this:
IMPORT_ASSERT_JS = re.compile(
    r"import {(.*)} from 'chrome://resources/ash/common/assert.js'")
# With this:
IMPORT_ASSERT_TS = '''import {%s} from 'chrome://resources/js/assert.js';\n'''

TS_IGNORE = re.compile(r'@ts-ignore')

COMMENT_JSDOC = re.compile(
    r'@params?|@returns?|@implements?|@type|@extends?|@private|@protected|@override'
)

# Matching:` this.bla;``  or `private this.bla;`
NON_INITIALIZED_PROP = re.compile(r'\s+this\.[\w_]+;')
# Matching: this.bla = 'anything';
INITIALIZED_PROP = re.compile(r'\s+this\.[\w_]+ = .*;')

DELETE_MARK = '-*- DELETE -*-'

# The location of the script.
_HERE = Path(__file__).resolve().parent
# Traverse back to the root of the repo.
_REPO_ROOT = _HERE.parent.parent.parent.parent
# //ui/file_manager/.
_FILE_MANAGER_ROOT = _REPO_ROOT.joinpath('ui', 'file_manager')
# //ui/file_manager/file_manager/.
_FILES_APP_ROOT = _FILE_MANAGER_ROOT.joinpath('file_manager')
# //ui/file_manager/integration_tests/.
_INTEGRATION_TESTS_ROOT = _FILE_MANAGER_ROOT.joinpath('integration_tests')
# //ui/file_manager/image_loader/.
_IMAGE_LOADER_ROOT = _FILE_MANAGER_ROOT.joinpath('image_loader')

#### Start of JS file conversions.


def is_comment(line):
    """Ignores inline comments."""

    l = line.strip()
    return (l.startswith('//') or ('/*' in l and '*/' not in l))


def is_jsdoc_star(line):
    return line.lstrip().startswith('*')


def process_js_file(js_fname):
    """Processes the JS file to convert the content to TS."""
    lines = []
    # True while processing a @ts-ignore comment, because it may span through
    # multiple lines.
    ts_ignoring = False

    with js_fname.open() as f:
        for line in f.readlines():
            # First pass we just remove lines.
            assert_import = IMPORT_ASSERT_JS.findall(line)
            if assert_import:
                names = assert_import[0]
                line = IMPORT_ASSERT_TS % names
                lines.append(line)
                continue

            # Fully commented out line, ignores inline comment.
            if is_comment(line):
                if ts_ignoring:
                    continue  # continue to delete a @ts-ignore.

                if TS_IGNORE.search(line):
                    ts_ignoring = True
                    continue  # delete the line.
            elif ts_ignoring:
                # End of the @ts-ignore.
                ts_ignoring = False

            lines.append(line)

    processing_comment = False
    idx = -1
    while (idx < len(lines) - 1):
        idx += 1
        line = lines[idx]

        if is_comment(line) or (processing_comment and is_jsdoc_star(line)):
            processing_comment = True
            match = COMMENT_JSDOC.search(line)

            original_line = line
            # Loop because we might need to process multiple JSDoc tags in the
            # same line.
            while (match):
                previous_line = line
                if not process_jsdoc(lines, line, idx, match):
                    print(f'*** Failed to process:\n{line}'
                          f'\nFrom:\n{original_line}')
                # Refresh the line:
                line = lines[idx]
                if (line == previous_line):
                    break
                match = COMMENT_JSDOC.search(line)
            continue

        if not is_jsdoc_star(line):
            processing_comment = False

    # Remove the lines marked for deletion.
    remove_lines(lines)
    return lines


def process_jsdoc(lines, cur_line, idx, regex_match):
    """Processes the `line` to convert the JSDoc to TS."""

    # Normalize the name without the `s`.
    jsdoc = cur_line[regex_match.start():regex_match.end()]
    if jsdoc.endswith('s'):
        jsdoc = jsdoc[:-1]

    if jsdoc == '@implement':
        process_extends_implements('implements', lines, idx)
        return True
    elif jsdoc == '@extend':
        process_extends_implements('extends', lines, idx)
        return True
    elif jsdoc == '@type':
        process_type(lines, idx)
        return True
    elif jsdoc == '@return':
        process_return(lines, idx)
        return True
    elif jsdoc == '@param':
        process_param(lines, idx)
        return True
    elif jsdoc == '@private':
        process_private('private', lines, idx)
        return True
    elif jsdoc == '@protected':
        process_private('protected', lines, idx)
        return True
    elif jsdoc == '@override':
        process_private('override', lines, idx)
        return True

    print(f'***Unknown JSDoc tag {jsdoc}')
    return False


def remove_lines(lines):
    """Removes the lines marked for deletion. Changes the list in-place."""
    idx = len(lines) - 1
    while idx >= 0:
        line = lines[idx]
        if DELETE_MARK in line:
            del lines[idx]
        idx -= 1


def maybe_remove_tag(tag, line):
    """Only removes the tag if the tag has no additional description."""
    tmp_line = remove_tag(tag, line)
    if DELETE_MARK in tmp_line:
        # Remove the tag and the line.
        return tmp_line

    # Return original line instead.
    return line


def remove_tag(tag, line):
    """Removes the JSDoc tag and if it doesn't have any additional comment mark
    the line for deletion."""
    ret = re.sub(tag, '', line)

    # Remove space `*` and spaces after `*`.
    maybe_empty_line = re.sub(r'^\s+\*\s*', '', ret)
    if not maybe_empty_line.strip():
        # Mark for deletion.
        return ret + DELETE_MARK

    # Return just removing the @tag.
    return ret


def extract_type(tag, line):
    """Extracts the type definition from the JSDoc tag.
    It removes the type definitions from the line."""
    try:
        t = re.findall(rf'{tag}\s+{{(.*)}}', line)[0]
        new_line = line.replace('{' + t + '}', '')
        return t, new_line
    except IndexError:
        return None, None


def get_param_name(line):
    """Gets the name of the param from the @param tag."""
    try:
        n = re.findall(r'@params?\s+(\w+)', line)[0]
        return n
    except IndexError:
        print(f'Failed to find the @param name in JSDoc on line:\n{line}')
        return ''


def sanitize_type(type_spec):
    """Converts the JSDoc definition to the TS definition."""
    t = type_spec.replace('!', '')
    t = t.replace('?', 'null|')
    t = t.replace('=', '')
    t = t.replace('*', 'unknown')
    t = t.replace('function():void', 'VoidCallback')
    t = re.sub(r'import\(.*\)\.', '', t)
    return t


def find_end_of_comment(new_file, idx):
    """Finds the first line from `idx` that isn't a comment."""
    while (idx < len(new_file)):
        line = new_file[idx]
        if is_comment(line) or is_jsdoc_star(line):
            idx += 1
            continue
        return idx


def process_type(lines, idx):
    """Converts the @type tag to TS."""
    line = lines[idx]
    t, new_line = extract_type(r'@type', line)
    if not t:
        return

    t = sanitize_type(t)
    new_line = remove_tag(r'@type', new_line)
    lines[idx] = new_line

    idx = find_end_of_comment(lines, idx)
    if not idx:
        return

    # Try to insert the type as TS syntax.
    new_line = lines[idx]
    if NON_INITIALIZED_PROP.search(new_line):
        # Case: this.bla;
        new_line = new_line.replace(';', f': {t};')
    elif INITIALIZED_PROP.search(new_line):
        # Case: this.bla = 'anything';
        new_line = new_line.replace(' =', f': {t} =')

    lines[idx] = new_line


def process_return(new_file, idx):
    """Converts the @return tag to TS."""
    line = new_file[idx]
    t, new_line = extract_type(r'@returns?', line)
    if not t:
        return

    t = sanitize_type(t)
    new_line = maybe_remove_tag(r'@returns?', new_line)
    if not DELETE_MARK in new_line:
        # Normalize @returns to @return (without S) and single space after it.
        new_line = re.sub(r'@returns?\s+', '@return ', new_line)

    new_file[idx] = new_line
    idx = find_end_of_comment(new_file, idx)
    if not idx:
        return

    # Try to insert the return type as TS syntax.
    # Find the beginning of the function scope.
    line = new_file[idx]
    while (' {\n' not in line):
        idx += 1
        if idx >= len(new_file):
            return
        line = new_file[idx]

    new_line = line.replace(' {\n', f': {t} {{\n', 1)
    new_file[idx] = new_line


def process_param(new_file, idx):
    """Converts the @param tag to TS."""
    line = new_file[idx]
    t, new_line = extract_type(r'@params?', line)
    if not t:
        return

    is_optional = t.endswith('=')
    param_name = get_param_name(new_line)
    new_line = maybe_remove_tag(fr'@params?\s+{param_name}', new_line)
    if not DELETE_MARK in new_line:
        # Normalize @params to @param (without S) and single space after it.
        new_line = re.sub(r'@params?\s+', '@param ', new_line)
    new_file[idx] = new_line

    t = sanitize_type(t)
    idx = find_end_of_comment(new_file, idx)
    if not idx:
        return

    # Try to insert the param type as TS syntax.
    # Find the param in the function signature.
    line = new_file[idx]
    search_param_name = re.compile(fr'(\W){param_name}(\W)')
    while (not search_param_name.search(line)):
        idx += 1
        if idx >= len(new_file):
            return
        line = new_file[idx]

    line = new_file[idx]
    new_param_name = param_name
    if is_optional:
        new_param_name += '?'
    new_line = search_param_name.sub(fr'\1{new_param_name}: {t}\2', line)
    new_file[idx] = new_line


def process_extends_implements(jsdoc_tag, new_file, idx):
    """Converts the @extends or @implements tag to TS."""
    tag_regex = r'@extends?'
    ts_keyword = 'extends'
    if jsdoc_tag == 'implements':
        tag_regex = r'@implements?'
        ts_keyword = 'implements'

    line = new_file[idx]
    # Extends doesn't have additional syntax like !?= for the type, so it
    # doesn't need the sanitize.
    t, new_line = extract_type(tag_regex, line)
    if not t:
        return

    new_line = remove_tag(tag_regex, new_line)
    new_file[idx] = new_line

    idx = find_end_of_comment(new_file, idx)
    if not idx:
        return
    line = new_file[idx]

    # Try to insert the implements/extends as TS syntax.
    if not ' class ' in line:
        return

    if f' {ts_keyword} ' in line:
        t += ', '
    else:
        t = f' {ts_keyword} {t}'

    new_line = re.sub(r'class (\w+)', rf'class \1{t}', line)
    new_file[idx] = new_line


def process_private(jsdoc_tag, new_file, idx):
    """Converts the @private or @protected tag to TS."""
    tag_regex = r'@private'
    ts_keyword = 'private'
    if jsdoc_tag == 'protected':
        tag_regex = r'@protected'
        ts_keyword = 'protected'
    elif jsdoc_tag == 'override':
        tag_regex = r'@override'
        ts_keyword = 'override'

    line = new_file[idx]

    new_line = remove_tag(tag_regex, line)
    new_file[idx] = new_line

    idx = find_end_of_comment(new_file, idx)
    if not idx:
        return
    line = new_file[idx]

    # Try to insert the private/protected as TS syntax.
    l = line.strip()
    if l.startswith('this.'):
        new_line = line.replace('this.', f'{ts_keyword} this.')
        new_file[idx] = new_line
        return

    new_line = re.sub(r'^(\s*)(\w+)', fr'\1{ts_keyword} \2', line)
    new_file[idx] = new_line


#### End of JS file conversions.

#### Start of Build file conversions:


def process_build_file(build_file_name, js_file_name):
    """Processes the BUILD file."""
    build_target = os.path.splitext(js_file_name.name)[0]
    new_file = []

    from_root = js_file_name.absolute().parent.relative_to(_REPO_ROOT)
    if build_file_name.parent == js_file_name.parent:
        # Within the same build file refers as ":bla".
        ref_target = f':{build_target}'
    else:
        # From other build files refer as "//path/to:bla".
        ref_target = f'//{from_root}:{build_target}'

    scope = 0
    with build_file_name.open() as f:
        for line in f.readlines():
            # A line that is referring to the removed build target, like a
            # `deps`.
            if ref_target in line:
                continue  # delete line

            # The start of the build target definitions.
            if (f'js_library("{build_target}") {{' in line
                    or f'js_unittest("{build_target}") {{' in line):
                scope += 1
                continue  # delete line

            # Detecting inner scope, like a `if` inside the `js_library()`.
            if scope > 0 and '{' in line and '}' not in line:
                scope += 1
                continue  # continue to delete in the scope.

            # End of a scope.
            if scope > 0 and '}' in line:
                scope -= 1
                continue  # delete the last line of the scope

            # End of all the scopes within the `js_library()`.
            if scope > 0:
                continue  # continue to delete in the scope.

            new_file.append(line)

    return new_file


def process_file_names_gni(js_fname, gni_file):
    """Removes the reference to the JS file and add to the TS file in the
    file_names.gni."""
    new_file = []
    # All files in file_names.gni a relative to //ui/file_manager.
    fname = js_fname.relative_to(_FILE_MANAGER_ROOT)
    js_str_name = str(fname)
    ts_str_name = js_str_name.replace('.js', '.ts')
    ts_line = f'  "{ts_str_name}",\n'

    # anchor is where the TS file will be added.
    anchor = 'ts_files = [\n'
    if js_str_name.endswith('unittest.js'):
        anchor = 'ts_test_files = [\n'

    with gni_file.open() as f:
        for line in f.readlines():
            if js_str_name in line:
                continue  # delete the line referring to the JS file.
            new_file.append(line)

            # The anchor was added above, just append the TS line.
            if line == anchor:
                new_file.append(ts_line)

    return new_file


def to_build_file(js_fname):
    """Converts the `js_fname` path to its sibling BUILD.gn."""
    return Path(js_fname).with_name('BUILD.gn')


def find_build_files(js_fname):
    """Finds the BUILD.gn files to process."""
    # Always process the BUILD.gn sibling of the JS file being converted.
    sibling_build = to_build_file(js_fname)
    ret = set()
    if sibling_build.exists():
        ret.add(Path(str(sibling_build)))
    str_js_path = str(js_fname)

    # Avoid mixing the BUILD.gn of the Files app, Integration Tests and Image
    # Loader.  If the JS file belongs to one of these, only process BUILD files
    # in that section.
    root = _FILES_APP_ROOT
    if str_js_path.startswith(str(_INTEGRATION_TESTS_ROOT)):
        root = _INTEGRATION_TESTS_ROOT
    elif str_js_path.startswith(str(_IMAGE_LOADER_ROOT)):
        root = _IMAGE_LOADER_ROOT

    for base, dirs, files in os.walk(root):
        for f in files:
            if f == 'BUILD.gn':
                ret.add(Path(base, f))

    return ret


#### End of Build file conversions:


def replace_file(fname, content):
    """Replaces the `fname` content in the file system."""
    with open(fname, 'w') as f:
        f.writelines(content)


def run_git_mv(js_path_str):
    """Runs the git mv to rename the JS file to TS."""
    ts_path = js_path_str.replace('.js', '.ts')
    cmd = [
        'git',
        'mv',
        js_path_str,
        ts_path,
    ]
    cmd = ' '.join(cmd)
    try:
        out = subprocess.check_output(cmd,
                                      stderr=subprocess.STDOUT,
                                      shell=True)
        return out.strip()
    except subprocess.CalledProcessError as e:
        print('***')
        print(e.stdout)
        print(e.stderr)
        return ''


#### Start of controlling the execution of the script.


def process_js_files(files):
    for fname in files:
        print('Processing:', fname)
        js_path = Path(fname)
        js_path_abs_str = str(js_path.absolute())

        # Processing all BUILD files
        bs = find_build_files(js_path)
        for b in bs:
            new_build_file = process_build_file(b, js_path)
            replace_file(b, new_build_file)

        # Only process file_names.gni for Files app:
        if js_path_abs_str.startswith(str(_FILES_APP_ROOT)):
            file_names = _FILE_MANAGER_ROOT.joinpath('file_names.gni')
            new_file_names_gni = process_file_names_gni(
                js_path.absolute(), file_names)
            replace_file(file_names, new_file_names_gni)

        # Process the JS file content and save as JS file.
        ts_content = process_js_file(js_path)
        replace_file(js_path, ts_content)

        # Rename to TS.
        run_git_mv(str(js_path))


def main():
    parser = argparse.ArgumentParser(
        description='Convert the given JS files to TS')
    parser.add_argument('js_files', metavar='FILE', type=str, nargs='+')
    args = parser.parse_args()

    process_js_files(args.js_files)


if __name__ == "__main__":
    main()
