# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A module to add gn support to cr."""

from __future__ import print_function

import cr
import os
import re

GN_ARG_PREFIX = 'GN_ARG_'


class GnPrepareOut(cr.PrepareOut):
  """A prepare action that runs gn whenever you select an output directory."""

  @property
  def priority(self):
    return -1

  def UpdateContext(self):
    # Collapse GN_ARGS from all GN_ARG prefixes.
    gn_args = cr.context.Find('GN_ARGS') or ''
    for key, value in cr.context.exported.items():
      if key.startswith(GN_ARG_PREFIX):
        gn_args += ' %s=%s' % (key[len(GN_ARG_PREFIX):], value)

    gn_args += (' is_debug=%s' %
        ('true' if cr.context['CR_BUILDTYPE'] == 'Debug' else 'false'))

    arch = cr.context.Find('CR_ENVSETUP_ARCH') or ''
    if arch:
      gn_args += ' target_cpu="%s"' % ('x86' if arch == 'ia32' else arch)

    # Detect goma.
    goma_binaries = cr.Host.SearchPath('gomacc', [
      '{GOMA_DIR}',
      '/usr/local/google/code/goma',
      os.path.expanduser('~/goma')
    ])
    if goma_binaries:
      gn_args += ' use_goma=true'
      gn_args += ' goma_dir="%s"' % os.path.dirname(goma_binaries[0])

    cr.context['GN_ARGS'] = gn_args.strip()
    if cr.context.verbose >= 1:
      print(cr.context.Substitute('GN_ARGS = {GN_ARGS}'))

  def Prepare(self):
    if cr.context.verbose >= 1:
      print(cr.context.Substitute('Invoking gn with {GN_ARGS}'))

    out_path = os.path.join(cr.context['CR_SRC'], cr.context['CR_OUT_FULL'])
    args_file = os.path.join(out_path, 'args.gn')
    args = {}
    # Split the argument list while preserving quotes,
    # e.g., a="b c" becomes ('a', '"b c"').
    split_re = r'(?:[^\s,"]|"(?:\\.|[^"])*")+'
    for arg in re.findall(split_re, cr.context['GN_ARGS']):
      key, value = arg.split('=', 1)
      args[key] = value

    # Override any existing settings.
    arg_lines = []
    if os.path.exists(args_file):
      with open(args_file) as f:
        for line in f:
          key = line.split('=', 1)[0].strip()
          if key not in args:
            arg_lines.append(line.strip())

    # Append new settings.
    for key, value in args.items():
      arg_lines.append('%s = %s' % (key, value))

    try:
      os.makedirs(out_path)
    except OSError:
      if not os.path.isdir(out_path):
        raise
    with open(args_file, 'w') as f:
      f.write('\n'.join(arg_lines) + '\n')

    cr.Host.Execute('gn', 'gen', out_path)
