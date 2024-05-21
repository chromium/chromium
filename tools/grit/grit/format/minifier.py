# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Framework for stripping whitespace and comments from resource files"""

from os import path
import subprocess
import sys

__js_minifier = None
__css_minifier = None

js_minifier_ignore_list = [
    # TODO(crbug.com/339686362): Excluded because Terser throws an error.
    'gen/chrome/browser/resources/omnibox/tsc/',
]


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
    for f in js_minifier_ignore_list:
      if f in filename:
        return source
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
