# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Inlines an HTML file into a JS (or TS) file at a location specified by a
# placeholder. This is useful for implementing Web Components using JS modules,
# where all the HTML needs to reside in the JS file (no more HTML imports).

import argparse
import sys
import io
from os import path, getcwd, makedirs

_CWD = getcwd()


def _add_template_markers(html_template):
  return '<!--_html_template_start_-->%s<!--_html_template_end_-->' % \
      html_template


def _extract_template(html_file):
  with io.open(html_file, encoding='utf-8', mode='r') as f:
    template = f.read()
    return _add_template_markers('\n' + template)


def process_v3_ready(js_file, html_file):
  # Extract HTML template and place in JS file.
  html_template = _extract_template(html_file)

  with io.open(js_file, encoding='utf-8') as f:
    lines = f.readlines()

  HTML_TEMPLATE_REGEX = '{__html_template__}'
  found = 0
  for i, line in enumerate(lines):
    if HTML_TEMPLATE_REGEX in line:
      found += 1
      line = line.replace(HTML_TEMPLATE_REGEX, html_template)
      lines[i] = line

  if found == 0:
    raise AssertionError('No HTML placeholder ' + HTML_TEMPLATE_REGEX +
                         ' found in ' + js_file)

  if found > 1:
    raise AssertionError('Multiple HTML placeholders ' + HTML_TEMPLATE_REGEX +
                         ' found in ' + js_file)

  out_filename = path.basename(js_file)
  return lines, out_filename


def main(argv):
  parser = argparse.ArgumentParser()
  parser.add_argument('--in_folder', required=True)
  parser.add_argument('--out_folder', required=True)
  parser.add_argument('--js_files', required=True, nargs="*")
  args = parser.parse_args(argv)

  in_folder = path.normpath(path.join(_CWD, args.in_folder))
  out_folder = path.normpath(path.join(_CWD, args.out_folder))

  results = []
  for js_file in args.js_files:
    # Detect file extension, since it can be either a .ts or .js file.
    extension = path.splitext(js_file)[1]
    html_file = js_file[:-len(extension)] + '.html'
    result = process_v3_ready(
        path.join(in_folder, js_file), path.join(in_folder, html_file))

    out_folder_for_file = path.join(out_folder, path.dirname(js_file))
    makedirs(out_folder_for_file, exist_ok=True)
    with io.open(path.join(out_folder, js_file), mode='wb') as f:
      for l in result[0]:
        f.write(l.encode('utf-8'))
  return


if __name__ == '__main__':
  main(sys.argv[1:])
