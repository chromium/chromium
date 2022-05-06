# Copyright 2022 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Generetes a wrapper TS file around a source CSS file holding a Polymer style
# module, or a Polymer <custom-style> holding CSS variable. Any metadata
# necessary for populating the wrapper file are provided in the form of special
# CSS comments. The ID of a style module is inferred from the filename, for
# example foo_style.css will be held in a module with ID 'foo-style'.

import argparse
import sys
import io
import re
from os import path, getcwd, makedirs

_CWD = getcwd()

_METADATA_START_REGEX = '#css_wrapper_metadata_start'
_METADATA_END_REGEX = '#css_wrapper_metadata_end'
_IMPORT_REGEX = '#import='
_INCLUDE_REGEX = '#include='
_TYPE_REGEX = '#type='

_STYLE_TEMPLATE = """import \'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js\';
%(imports)s

const styleMod = document.createElement(\'dom-module\');
styleMod.innerHTML = `
  <template>
    <style%(include)s>
%(content)s
    </style>
  </template>
`;
styleMod.register(\'%(id)s\');"""

_VARS_TEMPLATE = """import \'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js\';
%(imports)s

const $_documentContainer = document.createElement('template');
$_documentContainer.innerHTML = `
<custom-style>
  <style>
%(content)s
  </style>
</custom-style>
`;
document.head.appendChild($_documentContainer.content);"""


def _parse_style_line(line, parsed_data):
  if not parsed_data['include']:
    include_match = re.search(_INCLUDE_REGEX, line)
    if include_match:
      parsed_data['include'] = line[include_match.end():]

  import_match = re.search(_IMPORT_REGEX, line)
  if import_match:
    parsed_data['imports'].append(line[import_match.end():])


def _parse_vars_line(line, parsed_data):
  import_match = re.search(_IMPORT_REGEX, line)
  if import_match:
    parsed_data['imports'].append(line[import_match.end():])


def _extract_data(css_file):
  metadata_start_line = -1
  metadata_end_line = -1

  parsed_data = {'type': None}

  with io.open(css_file, encoding='utf-8', mode='r') as f:
    lines = f.read().splitlines()

    for i, line in enumerate(lines):
      if metadata_start_line == -1:
        if _METADATA_START_REGEX in line:
          assert metadata_end_line == -1
          metadata_start_line = i
      else:
        assert metadata_end_line == -1

        if not parsed_data['type']:
          type_match = re.search(_TYPE_REGEX, line)
          if type_match:
            type = line[type_match.end():]
            assert type in ['style', 'vars']

            if type == 'style':
              id = path.splitext(path.basename(css_file))[0].replace('_', '-')
              parsed_data = {
                  'content': None,
                  'id': id,
                  'imports': [],
                  'include': None,
                  'type': type,
              }
            elif type == 'vars':
              parsed_data = {
                  'content': None,
                  'imports': [],
                  'type': type,
              }

        elif parsed_data['type'] == 'style':
          _parse_style_line(line, parsed_data)
        elif parsed_data['type'] == 'vars':
          _parse_vars_line(line, parsed_data)

        if _METADATA_END_REGEX in line:
          assert metadata_start_line > -1
          metadata_end_line = i
          parsed_data['content'] = '\n'.join(lines[metadata_end_line + 1:])
          break

    assert metadata_start_line > -1
    assert metadata_end_line > -1
    assert parsed_data['content']

    return parsed_data


def main(argv):
  parser = argparse.ArgumentParser()
  parser.add_argument('--in_folder', required=True)
  parser.add_argument('--out_folder', required=True)
  parser.add_argument('--in_files', required=True, nargs="*")
  args = parser.parse_args(argv)

  in_folder = path.normpath(path.join(_CWD, args.in_folder))
  out_folder = path.normpath(path.join(_CWD, args.out_folder))
  extension = '.ts'

  def _urls_to_imports(urls):
    return '\n'.join(map(lambda i: f'import \'{i}\';', urls))

  for in_file in args.in_files:
    parsed_data = _extract_data(path.join(in_folder, in_file))

    wrapper = None
    if parsed_data['type'] == 'style':
      include = ''
      if parsed_data['include']:
        parsed_include = parsed_data['include']
        include = f' include="{parsed_include}"'

      wrapper = _STYLE_TEMPLATE % {
          'imports': _urls_to_imports(parsed_data['imports']),
          'content': parsed_data['content'],
          'include': include,
          'id': parsed_data['id'],
      }
    elif parsed_data['type'] == 'vars':
      wrapper = _VARS_TEMPLATE % {
          'imports': _urls_to_imports(parsed_data['imports']),
          'content': parsed_data['content'],
      }

    assert wrapper

    out_folder_for_file = path.join(out_folder, path.dirname(in_file))
    makedirs(out_folder_for_file, exist_ok=True)
    with io.open(path.join(out_folder, in_file) + extension, mode='wb') as f:
      f.write(wrapper.encode('utf-8'))
  return


if __name__ == '__main__':
  main(sys.argv[1:])
