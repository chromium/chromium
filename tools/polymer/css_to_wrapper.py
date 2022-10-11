# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Generetes a wrapper TS file around a source CSS file holding a Polymer style
# module, or a Polymer <custom-style> holding CSS variable. Any metadata
# necessary for populating the wrapper file are provided in the form of special
# CSS comments. The ID of a style module is inferred from the filename, for
# example foo_style.css will be held in a module with ID 'foo-style'.

import argparse
import io
import re
import shutil
import sys
import tempfile
from os import path, getcwd, makedirs

_HERE_PATH = path.dirname(__file__)
_SRC_PATH = path.normpath(path.join(_HERE_PATH, '..', '..'))
_CWD = getcwd()

sys.path.append(path.join(_SRC_PATH, 'third_party', 'node'))
import node

_METADATA_START_REGEX = '#css_wrapper_metadata_start'
_METADATA_END_REGEX = '#css_wrapper_metadata_end'
_IMPORT_REGEX = '#import='
_INCLUDE_REGEX = '#include='
_SCHEME_REGEX = '#scheme='
_TYPE_REGEX = '#type='

_STYLE_TEMPLATE = """import {html} from \'%(scheme)s//resources/polymer/v3_0/polymer/polymer_bundled.min.js\';
%(imports)s

const styleMod = document.createElement(\'dom-module\');
styleMod.appendChild(html`
  <template>
    <style%(include)s>
%(content)s
    </style>
  </template>
`.content);
styleMod.register(\'%(id)s\');"""

_VARS_TEMPLATE = """import {html} from \'%(scheme)s//resources/polymer/v3_0/polymer/polymer_bundled.min.js\';
%(imports)s

const template = html`
<custom-style>
  <style>
%(content)s
  </style>
</custom-style>
`;
document.head.appendChild(template.content);"""


def _parse_style_line(line, metadata):
  include_match = re.search(_INCLUDE_REGEX, line)
  if include_match:
    assert not metadata[
        'include'], f'Found multiple "{_INCLUDE_REGEX}" lines. Only one should exist.'
    metadata['include'] = line[include_match.end():]

  import_match = re.search(_IMPORT_REGEX, line)
  if import_match:
    metadata['imports'].append(line[import_match.end():])


def _parse_vars_line(line, metadata):
  import_match = re.search(_IMPORT_REGEX, line)
  if import_match:
    metadata['imports'].append(line[import_match.end():])


def _extract_content(css_file, metadata, minified):
  with io.open(css_file, encoding='utf-8', mode='r') as f:
    # If minification is on, just grab the result from html-minifier's output.
    if minified:
      return f.read()

    # If minification is off, strip the special metadata comments from the
    # original file.
    lines = f.read().splitlines()
    return '\n'.join(lines[metadata['metadata_end_line'] + 1:])


def _extract_metadata(css_file):
  metadata_start_line = -1
  metadata_end_line = -1

  metadata = {'type': None, 'scheme': 'default'}

  with io.open(css_file, encoding='utf-8', mode='r') as f:
    lines = f.read().splitlines()

    for i, line in enumerate(lines):
      if metadata_start_line == -1:
        if _METADATA_START_REGEX in line:
          assert metadata_end_line == -1
          metadata_start_line = i
      else:
        assert metadata_end_line == -1

        if not metadata['type']:
          type_match = re.search(_TYPE_REGEX, line)
          if type_match:
            type = line[type_match.end():]
            assert type in ['style', 'vars']

            if type == 'style':
              id = path.splitext(path.basename(css_file))[0].replace('_', '-')
              metadata = {
                  'id': id,
                  'imports': [],
                  'include': None,
                  'metadata_end_line': -1,
                  'scheme': metadata['scheme'],
                  'type': type,
              }
            elif type == 'vars':
              metadata = {
                  'imports': [],
                  'metadata_end_line': -1,
                  'scheme': metadata['scheme'],
                  'type': type,
              }

        elif metadata['type'] == 'style':
          _parse_style_line(line, metadata)
        elif metadata['type'] == 'vars':
          _parse_vars_line(line, metadata)

        if metadata['scheme'] == 'default':
          scheme_match = re.search(_SCHEME_REGEX, line)
          if scheme_match:
            scheme = line[scheme_match.end():]
            assert scheme in ['chrome', 'relative']
            metadata['scheme'] = scheme

        if _METADATA_END_REGEX in line:
          assert metadata_start_line > -1
          metadata_end_line = i
          metadata['metadata_end_line'] = metadata_end_line
          break

    assert metadata_start_line > -1
    assert metadata_end_line > -1

    return metadata


def main(argv):
  parser = argparse.ArgumentParser()
  parser.add_argument('--in_folder', required=True)
  parser.add_argument('--out_folder', required=True)
  parser.add_argument('--in_files', required=True, nargs="*")
  parser.add_argument('--minify', action='store_true')
  parser.add_argument('--use_js', action='store_true')
  args = parser.parse_args(argv)

  in_folder = path.normpath(path.join(_CWD, args.in_folder))
  out_folder = path.normpath(path.join(_CWD, args.out_folder))
  extension = '.js' if args.use_js else '.ts'

  # The folder to be used to read the CSS files to be wrapped.
  wrapper_in_folder = in_folder

  if args.minify:
    # Minify the CSS files with html-minifier before generating the wrapper
    # .ts files.
    # Note: Passing all CSS files to html-minifier all at once because
    # passing them individually takes a lot longer.
    # Storing the output in a temporary folder, which is used further below when
    # creating the final wrapper files.
    tmp_out_dir = tempfile.mkdtemp(dir=out_folder)
    try:
      wrapper_in_folder = tmp_out_dir

      # Using the programmatic Node API to invoke html-minifier, because the
      # built-in command line API does not support explicitly specifying
      # multiple files to be processed, and only supports specifying an input
      # folder, which would lead to potentially processing unnecessary HTML
      # files that are not part of the build (stale), or handled by other
      # css_to_wrapper targets.
      node.RunNode(
          [path.join(_HERE_PATH, 'html_minifier.js'), in_folder, tmp_out_dir] +
          args.in_files)
    except RuntimeError as err:
      shutil.rmtree(tmp_out_dir)
      raise err

  def _urls_to_imports(urls):
    return '\n'.join(map(lambda i: f'import \'{i}\';', urls))

  for in_file in args.in_files:
    # Extract metadata from the original file, as the special metadata comments
    # only exist there.
    metadata = _extract_metadata(path.join(in_folder, in_file))

    # Extract the CSS content from either the original or the minified files.
    content = _extract_content(path.join(wrapper_in_folder, in_file), metadata,
                               args.minify)

    # Extract the URL scheme that should be used for absolute URL imports.
    scheme = None
    if metadata['scheme'] in ['default', 'chrome']:
      scheme = 'chrome:'
    elif metadata['scheme'] == 'relative':
      scheme = ''

    wrapper = None
    if metadata['type'] == 'style':
      include = ''
      if metadata['include']:
        parsed_include = metadata['include']
        include = f' include="{parsed_include}"'

      wrapper = _STYLE_TEMPLATE % {
          'imports': _urls_to_imports(metadata['imports']),
          'content': content,
          'include': include,
          'id': metadata['id'],
          'scheme': scheme,
      }
    elif metadata['type'] == 'vars':
      wrapper = _VARS_TEMPLATE % {
          'imports': _urls_to_imports(metadata['imports']),
          'content': content,
          'scheme': scheme,
      }

    assert wrapper

    out_folder_for_file = path.join(out_folder, path.dirname(in_file))
    makedirs(out_folder_for_file, exist_ok=True)
    with io.open(path.join(out_folder, in_file) + extension, mode='wb') as f:
      f.write(wrapper.encode('utf-8'))

  if args.minify:
    # Delete the temporary folder that was holding minified CSS files, no
    # longer needed.
    shutil.rmtree(tmp_out_dir)

  return


if __name__ == '__main__':
  main(sys.argv[1:])
