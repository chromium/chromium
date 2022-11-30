#!/usr/bin/env python
# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Generates shim headers that mirror the directory structure of bundled headers,
but just forward to the system ones.

This allows seamless compilation against system headers with no changes
to our source code.
"""


import optparse
import os.path
import sys


SHIM_TEMPLATE = """
#if defined(OFFICIAL_BUILD)
#error shim headers must not be used in official builds!
#endif
"""


def GeneratorMain(argv):
  parser = optparse.OptionParser()
  parser.add_option('--headers-root', action='append')
  parser.add_option('--define', action='append')
  parser.add_option('--output-directory')
  parser.add_option('--prefix', default='')
  parser.add_option('--use-include-next', action='store_true')
  parser.add_option('--outputs', action='store_true')
  parser.add_option('--generate', action='store_true')

  options, args = parser.parse_args(argv)

  if not options.headers_root:
    parser.error('Missing --headers-root parameter.')
  if not options.output_directory:
    parser.error('Missing --output-directory parameter.')
  if not args:
    parser.error('Missing arguments - header file names.')

  source_tree_root = os.path.abspath(
    os.path.join(os.path.dirname(__file__), '..', '..'))

  for root in options.headers_root:
    target_directory = os.path.join(
      options.output_directory,
      os.path.relpath(root, source_tree_root))
    if options.generate and not os.path.exists(target_directory):
      os.makedirs(target_directory)

    for header_spec in args:
      if ';' in header_spec:
        (header_filename,
         include_before,
         include_after) = header_spec.split(';', 2)
      else:
        header_filename = header_spec
        include_before = ''
        include_after = ''
      if options.outputs:
        yield os.path.join(target_directory, header_filename)
      if options.generate:
        header_path = os.path.join(target_directory, header_filename)
        header_dir = os.path.dirname(header_path)
        if not os.path.exists(header_dir):
          os.makedirs(header_dir)
        with open(header_path, 'w') as f:
          f.write(SHIM_TEMPLATE)

          if options.define:
            for define in options.define:
              key, value = define.split('=', 1)
              # This non-standard push_macro extension is supported
              # by compilers we support (GCC, clang).
              f.write('#pragma push_macro("%s")\n' % key)
              f.write('#undef %s\n' % key)
              f.write('#define %s %s\n' % (key, value))

          if include_before:
            for header in include_before.split(':'):
              f.write('#include %s\n' % header)

          include_target = options.prefix + header_filename
          if options.use_include_next:
            f.write('#include_next <%s>\n' % include_target)
          else:
            f.write('#include <%s>\n' % include_target)

          if include_after:
            for header in include_after.split(':'):
              f.write('#include %s\n' % header)

          if options.define:
            for define in options.define:
              key, value = define.split('=', 1)
              # This non-standard pop_macro extension is supported
              # by compilers we support (GCC, clang).
              f.write('#pragma pop_macro("%s")\n' % key)


def DoMain(argv):
  return '\n'.join(GeneratorMain(argv))


if __name__ == '__main__':
  DoMain(sys.argv[1:])
