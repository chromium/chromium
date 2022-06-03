# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Framework for stripping whitespace and comments from resource files"""

from __future__ import print_function

from os import path
import subprocess
import sys

import six

__js_minifier = None
__css_minifier = None

def SetJsMinifier(minifier):
  global __js_minifier
  __js_minifier = minifier.split()

def SetCssMinifier(minifier):
  global __css_minifier
  __css_minifier = minifier.split()

def Minify(source, filename):
  """Minify |source| (bytes) from |filename| and return bytes."""
  file_type = path.splitext(filename)[1]
  minifier = None
  if file_type == '.js':
    minifier = __js_minifier
  elif file_type == '.css':
    minifier = __css_minifier
  if not minifier:
    return source
  p = subprocess.Popen(
      minifier,
      stdin=subprocess.PIPE,
      stdout=subprocess.PIPE,
      stderr=subprocess.PIPE)
  (stdout, stderr) = p.communicate(source)
  if p.returncode != 0:
    print('Minification failed for %s' % filename)
    print(stderr)
    sys.exit(p.returncode)
  return stdout
