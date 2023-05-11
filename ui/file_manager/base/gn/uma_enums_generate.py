#!/usr/bin/env python3

# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
This file generates UMA enums to be used by JS and C++ from enums.xml.
"""

import os
import sys
import xml.etree.ElementTree as ET

# Import jinja2 from //third_party/jinja2
current_dir = os.path.dirname(__file__)
src_dir = os.path.join(current_dir, '../../../..')
sys.path.insert(1, os.path.join(src_dir, 'third_party'))
import jinja2

root = ET.parse(os.path.join(src_dir, 'tools/metrics/histograms/enums.xml'))
e = root.find('.//enum[@name="ViewFileType"]')
enums = [x.attrib['label'] for x in e]

env = jinja2.Environment(loader=jinja2.FileSystemLoader(current_dir),
                         lstrip_blocks=True,
                         trim_blocks=True)


def render(template, result_file):
    template = env.get_template(template)
    with open(os.path.join(src_dir, result_file), 'w') as f:
        f.write(template.render({'enums': enums}))


render('uma_enums.ts.jinja',
       'ui/file_manager/file_manager/foreground/js/uma_enums.gen.ts')
render('uma_enums.h.jinja', 'chrome/browser/ash/file_manager/uma_enums.gen.h')
render('uma_enums.cc.jinja',
       'chrome/browser/ash/file_manager/uma_enums.gen.cc')

print(
    'Done. Format using: git cl format && git cl format --js ui/file_manager')
