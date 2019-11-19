#!/usr/bin/env python
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import print_function

from bs4 import BeautifulSoup
from datetime import date
import os.path as path
import sys

_COMPILE_JS = '//third_party/closure_compiler/compile_js.gni'
_POLYMERS = ['polymer.html', 'polymer-mini.html', 'polymer-micro.html']
_COMPILED_RESOURCES_TEMPLATE = '''
# Copyright %d The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# NOTE: Created with %s, please do not edit.

import("%s")

%s
'''.strip()


def main(created_by, html_files):
    targets = ''

    def _html_to_extracted(html_file):
        assert html_file.endswith('.html')
        return html_file[:-len('.html')] + '-extracted'

    def _target_name(target_file):
        return _html_to_extracted(path.basename(target_file))

    def _has_extracted_js(html_file):
        return path.isfile(_html_to_extracted(html_file) + '.js')

    html_files = filter(_has_extracted_js, html_files)

    for html_file in sorted(html_files, key=_target_name):
        html_base = path.basename(html_file)
        if html_base in _POLYMERS:
            continue

        parsed = BeautifulSoup(open(html_file), 'html.parser')
        imports = set(
            i.get('href') for i in parsed.find_all('link', rel='import'))

        html_dir = path.dirname(html_file)
        dependencies = []
        externs = ''

        for html_import in sorted(imports):
            import_dir, import_base = path.split(html_import.encode('ascii'))
            if import_base in _POLYMERS:
                continue

            # Only exclude these after appending web animations externs.
            if not _has_extracted_js(path.join(html_dir, html_import)):
                continue

            target = ':' + _target_name(import_base)

            dependencies.append(import_dir + target)

        targets += '\njs_library("%s-extracted") {' % html_base[:-5]
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
