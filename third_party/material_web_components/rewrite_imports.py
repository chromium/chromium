# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import os
import re
import sys
import json

from pathlib import Path

_HERE_DIR = Path(__file__).parent
_SOURCE_MAP_CREATOR = (_HERE_DIR / 'rewrite_imports.mjs').resolve()

_NODE_PATH = (_HERE_DIR.parent.parent / 'third_party' / 'node').resolve()
sys.path.append(str(_NODE_PATH))
import node

_CWD = os.getcwd()

# TODO(crbug.com/1320176): Consider either integrating this functionality into
# ts_library.py or replacing the regex if only "tslib" is ever rewritten.
def main(argv):
    parser = argparse.ArgumentParser()
    # List of imports and what they should be rewritten to. Specified as an
    # array of "src|dest" strings. Note that if the src is not a directory, it
    # will be treated as a regex matched against the entire import path. I.e.
    # "foo|bar" will translate to re.sub("^foo$", "bar", line). Since re.sub is
    # used, regex features like referencing groups from `src` in `dest` are
    # available.
    parser.add_argument('--import_mappings', nargs='*')
    # List of rules for renaming imported variables. The format is
    # "path:oldName|newName". When "path" is part of the original path, the
    # variable is renamed to "newName". E.g.
    # Rule: 'lit/static-html:html|staticHtml'
    # Original: import { static } from 'lit/static-html'
    # Rewrite: import { staticHtml } from 'lit/static-html'
    parser.add_argument('--import_var_mappings', nargs='*')
    # The directory to output the rewritten files to.
    parser.add_argument('--out_dir', required=True)
    # The directory to output the manifest file to. The manifest can be used
    # by downstream tools (such as generate_grd) to capture all files that were
    # rewritten.
    parser.add_argument('--manifest_out', required=True)
    # The directory all in_files are under, used to construct a real path on
    # disk via base_dir + in_file.
    parser.add_argument('--base_dir', required=True)
    # List of files to rewrite imports for, all files should be provided
    # relative to base_dir. Each files path is preserved when outputed
    # into out_dir. I.e. "a/b/c/foo" will be outputted to "base_dir/a/b/c/foo".
    parser.add_argument('--in_files', nargs='*')
    args = parser.parse_args(argv)

    manifest = {'base_dir': args.out_dir, 'files': []}
    import_mappings = dict()
    for mapping in args.import_mappings:
        (src, dst) = mapping.split('|')
        import_mappings[src] = dst

    import_var_mappings = list()
    for mapping in args.import_var_mappings:
        (path, renaming) = mapping.split(':')
        (old_name, new_name) = renaming.split('|')
        import_var_mappings.append((path, (old_name, new_name)))

    # For `import_path`, either replace the prefix as described in
    #  `--import_mappings` or drop the "generated:" prefix.
    def _map_import(import_path):
        for regex, substitution in import_mappings.items():
            if regex[-1] == "/":
                substitution = rf"{substitution}\g<file>"
            rewritten = re.sub(rf"^{regex}(?P<file>.*)", substitution,
                               import_path)
            if rewritten != import_path:
                return rewritten

        return re.sub(r'^generated:(.*)', r'\g<1>', import_path)

    # Applies the rules from --import_var_mappings and returns the rewritten
    # import variables.
    def _map_import_vars(path, variables):
        for (map_path, (old, new)) in import_var_mappings:
            if map_path in path:
                # Rewrite direct imports:
                # import {html} from "lit/static-html" -->
                # import {staticHtml as html} from "lit/static-html"
                variables = re.sub(rf"\b({old})(\s*[,}}])", rf"{new} as \1\2",
                                   variables)
                # Rewrite aliased imports:
                # import {html as foo} from "lit/static-html" -->
                # import {staticHtml as foo} from "lit/static-html"
                variables = re.sub(rf"\b{old} (as \w+)", rf"{new} \1",
                                   variables)
                # Cleanup aliases:
                # import {staticHtml as staticHtml} from "lit/static-html" -->
                # import {staticHtml} from "lit/static-html"
                variables = variables.replace(f'{new} as {new}', new)
        return variables

    for f in args.in_files:
        output = []
        changed_lines_list = list()
        for line_no, line in enumerate(
                open(os.path.join(args.base_dir, f), 'r').readlines()):
            # Investigate JS parsing if this is insufficient.
            match = re.match(r'^(import .*["\'])(.*)(["\'];)$', line)
            if match:
                import_vars = match.group(1)
                import_path = match.group(2)
                new_import_path = _map_import(import_path)
                new_import_vars = _map_import_vars(import_path, import_vars)
                # If this is an import statement line and it has a replacement,
                # modify the line before outputing it.
                if new_import_path != import_path or new_import_vars != import_vars:
                    line = f"{new_import_vars}{new_import_path}{match.group(3)}\n"
                    generated_column = len(import_vars) + len(import_path)
                    # TODO(b/290142486): Also adjust the location of import var
                    # tokens.
                    rewritten_column = len(new_import_vars) + len(
                        new_import_path)
                    changed_line = {
                        "lineNum": line_no,
                        "generatedColumn": generated_column,
                        "rewrittenColumn": rewritten_column
                    }
                    changed_lines_list.append(changed_line)
            output.append(line)
        with open(os.path.join(args.out_dir, f), 'w') as out_file:
            out_file.write(''.join(output))
        manifest['files'].append(f)

        node.RunNode([str(_SOURCE_MAP_CREATOR), args.base_dir, f, args.out_dir, json.dumps(changed_lines_list)])

    with open(args.manifest_out, 'w') as manifest_file:
        json.dump(manifest, manifest_file)


if __name__ == '__main__':
    main(sys.argv[1:])
