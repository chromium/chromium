#!/usr/bin/env python
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import print_function

from datetime import date
import json
import os.path as path
import sys

_COMPILE_JS = '//third_party/closure_compiler/compile_js.gni'
_COMPILED_RESOURCES_TEMPLATE = '''
# Copyright %d The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# NOTE: Created with %s, please do not edit.

import("%s")

%s
'''.strip()

_HERE_PATH = path.dirname(__file__)
_SRC_PATH = path.normpath(path.join(_HERE_PATH, '..', '..'))
sys.path.append(path.join(_SRC_PATH, 'third_party', 'node'))
import node


def main(created_by, input_files):
    targets = ''

    def _target_name(target_file):
      return target_file[:-len('.js')]

    def _extract_imports(input_file):
      path_to_acorn = path.join('node_modules', 'acorn', 'bin', 'acorn');
      ast = node.RunNode([path_to_acorn, '--module', input_file])
      imports = map(lambda n: n['source']['raw'][1:-1],
          filter(lambda n: n['type'] ==
              'ImportDeclaration', json.loads(ast)['body']))
      return set(imports)

    for input_file in sorted(input_files, key=_target_name):
      input_base = path.basename(input_file)
      imports = _extract_imports(input_file)
      dependencies = []
      externs = ''

      for i in sorted(imports):
        import_dir, import_base = path.split(i.encode('ascii'))

        # Redirect dependencies to minified Polymer to the non-minified version.
        if import_base == 'polymer_bundled.min.js':
          import_base = 'polymer_bundled.js'

        target = ':' + _target_name(import_base)
        dependencies.append(import_dir + target)

      targets += '\njs_library("%s") {' % _target_name(input_base)
      if dependencies:
        targets += '\n  deps = ['
        targets += '\n    "%s",' % '",\n    "'.join(dependencies)
        targets += '\n  ]'
      targets += externs
      targets += '\n}\n'

    targets = targets.strip()

    if targets:
      current_year = date.today().year
      print(_COMPILED_RESOURCES_TEMPLATE % (current_year, created_by,
                                            _COMPILE_JS, targets))


if __name__ == '__main__':
  main(path.basename(sys.argv[0]), sys.argv[1:])
