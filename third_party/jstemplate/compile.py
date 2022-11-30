#!/usr/bin/env python
# Copyright 2011 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Combines the javascript files needed by jstemplate into a single file."""

import httplib
import sys
import urllib


def main():
  srcs = ['util.js', 'jsevalcontext.js', 'jstemplate.js', 'exports.js']
  out = 'jstemplate_compiled.js'

  # Wrap the output in an anonymous function to prevent poluting the global
  # namespace.
  output_wrapper = '(function(){%s})()'

  # Define the parameters for the POST request and encode them in a URL-safe
  # format. See http://code.google.com/closure/compiler/docs/api-ref.html for
  # API reference.
  params = urllib.urlencode(
    map(lambda src: ('js_code', file(src).read()), srcs) +
    [
      ('compilation_level', 'ADVANCED_OPTIMIZATIONS'),
      ('output_format', 'text'),
      ('output_info', 'compiled_code'),
    ])

  # Always use the following value for the Content-type header.
  headers = {'Content-type': 'application/x-www-form-urlencoded'}
  conn = httplib.HTTPSConnection('closure-compiler.appspot.com')
  conn.request('POST', '/compile', params, headers)
  response = conn.getresponse()
  out_file = file(out, 'w')
  out_file.write(output_wrapper % response.read())
  out_file.close()
  conn.close()
  return 0


if __name__ == '__main__':
  sys.exit(main())
