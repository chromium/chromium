#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import json
import os
import re
import sys

_HEAD_TAG_RE = re.compile(r'<head\b[^>]*>', re.IGNORECASE)
_HEAD_END_RE = re.compile(r'</head>', re.IGNORECASE)
_HTML_COMMENT_RE = re.compile(r'<!--.*?-->', re.DOTALL)
_FIRST_SCRIPT_RE = re.compile(
  r'<script\b|<link\b[^>]*\brel=["\']?modulepreload\b', re.IGNORECASE
)

_TEST_LOADER_TEMPLATE = """<!DOCTYPE html>
<html>
<head>
  <script type="importmap">%(import_map)s</script>
</head>
<body>
  <script src="//webui-test/mocha.js"></script>
  <script type="module" src="test_loader.js"></script>
</body>
</html>"""

_IMPORTMAP_TEMPLATE = '<script type="importmap">%(import_map)s</script>\n'

_CWD = os.getcwd()


def _iter_manifest_imports(manifest_path):
  with open(manifest_path, 'r', encoding='utf-8') as f:
    data = json.load(f)
  for entry in data.get('entries', [data]):
    import_path_prefix = (
      entry.get('import_path_prefix', '').replace('\\', '/').strip('/')
    )
    raw_serve_prefix = entry.get('serve_path_prefix', '').replace('\\', '/')
    if raw_serve_prefix:
      assert raw_serve_prefix != '//' and raw_serve_prefix.startswith(
        ('/', './', '../', 'chrome://', 'chrome-untrusted://')
      ), f'Invalid serve_path_prefix: {raw_serve_prefix}'
    serve_path_prefix = raw_serve_prefix.rstrip('/')
    for file_name in entry['files']:
      file_name = file_name.replace('\\', '/').lstrip('/')
      import_path = (
        f'{import_path_prefix}/{file_name}' if import_path_prefix else file_name
      )
      # Use `serve_path_prefix` when files are served from a different runtime
      # location than `/<import_path>` (e.g. `//resources/mojo`).
      target_url = (
        f'{serve_path_prefix}/{file_name}'
        if raw_serve_prefix
        else f'/{import_path}'
      )
      yield import_path, target_url


def _add_import_mapping(imports, import_path, target_url):
  if import_path in imports:
    assert imports[import_path] == target_url, (
      f'Conflicting import map entries for {import_path}: '
      f'{imports[import_path]} vs {target_url}'
    )
    return
  imports[import_path] = target_url


def main(argv):
  parser = argparse.ArgumentParser()
  parser.add_argument('--manifest_files', nargs='*', default=[])
  parser.add_argument('--in_files', nargs='+', required=True)
  parser.add_argument('--in_folder', required=True)
  parser.add_argument('--out_folder', required=True)
  parser.add_argument('--out_manifest')
  args = parser.parse_args(argv)

  in_folder = os.path.normpath(os.path.join(_CWD, args.in_folder))
  out_folder = os.path.normpath(os.path.join(_CWD, args.out_folder))

  import_map = {'imports': {}}
  for manifest in args.manifest_files:
    manifest_path = os.path.normpath(os.path.join(_CWD, manifest))
    for import_path, target_url in _iter_manifest_imports(manifest_path):
      _add_import_mapping(import_map['imports'], import_path, target_url)

  import_map_json = json.dumps(import_map)
  import_map_html = _IMPORTMAP_TEMPLATE % {'import_map': import_map_json}

  # Inject the import map inside <head> before the first <script> (or
  # <link rel="modulepreload">) if present so module specifiers resolve against
  # the map, while remaining after <meta charset> and <base> tags; otherwise
  # inject right before </head>.
  for html_file in args.in_files:
    in_file = os.path.join(in_folder, html_file)
    with open(in_file, 'r', encoding='utf-8') as f:
      html_content = f.read()

    uncommented_html = _HTML_COMMENT_RE.sub(
      lambda m: ' ' * len(m.group(0)), html_content
    )
    head_match = _HEAD_TAG_RE.search(uncommented_html)
    head_end_match = (
      _HEAD_END_RE.search(uncommented_html, head_match.end())
      if head_match
      else None
    )
    assert head_match and head_end_match, (
      f'Expected "<head>...</head>" in {in_file} to inject import map.'
    )
    first_script_match = _FIRST_SCRIPT_RE.search(
      uncommented_html, head_match.end(), head_end_match.start()
    )
    insert_pos = (
      first_script_match.start()
      if first_script_match
      else head_end_match.start()
    )
    output = (
      html_content[:insert_pos] + import_map_html + html_content[insert_pos:]
    )

    out_file = os.path.join(out_folder, html_file)
    os.makedirs(os.path.dirname(out_file), exist_ok=True)
    with open(out_file, 'w', encoding='utf-8', newline='') as f:
      f.write(output)

  # Generate test_loader.html using the template and the map.
  test_loader_html = _TEST_LOADER_TEMPLATE % {'import_map': import_map_json}
  test_loader_file = os.path.join(out_folder, 'test_loader.html')
  with open(test_loader_file, 'w', encoding='utf-8', newline='') as f:
    f.write(test_loader_html)

  if args.out_manifest:
    manifest_data = {
      'base_dir': args.out_folder.replace('\\', '/'),
      'files': [f.replace('\\', '/') for f in args.in_files]
      + ['test_loader.html'],
    }
    out_manifest_path = os.path.normpath(os.path.join(_CWD, args.out_manifest))
    os.makedirs(os.path.dirname(out_manifest_path), exist_ok=True)
    with open(out_manifest_path, 'w', encoding='utf-8', newline='') as f:
      json.dump(manifest_data, f)


if __name__ == '__main__':
  main(sys.argv[1:])
