#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Command processor for GRIT.  This is the script you invoke to run the various
GRIT tools.
"""

from __future__ import print_function

import os
import sys
if __name__ == '__main__':
  sys.path.append(os.path.join(os.path.dirname(__file__), '..'))

import getopt

from grit import util

import grit.extern.FP

# Tool info factories; these import only within each factory to avoid
# importing most of the GRIT code until required.
def ToolFactoryBuild():
  import grit.tool.build
  return grit.tool.build.RcBuilder()

def ToolFactoryBuildInfo():
  import grit.tool.buildinfo
  return grit.tool.buildinfo.DetermineBuildInfo()

def ToolFactoryCount():
  import grit.tool.count
  return grit.tool.count.CountMessage()

def ToolFactoryDiffStructures():
  import grit.tool.diff_structures
  return grit.tool.diff_structures.DiffStructures()

def ToolFactoryMenuTranslationsFromParts():
  import grit.tool.menu_from_parts
  return grit.tool.menu_from_parts.MenuTranslationsFromParts()

def ToolFactoryNewGrd():
  import grit.tool.newgrd
  return grit.tool.newgrd.NewGrd()

def ToolFactoryResizeDialog():
  import grit.tool.resize
  return grit.tool.resize.ResizeDialog()

def ToolFactoryRc2Grd():
  import grit.tool.rc2grd
  return grit.tool.rc2grd.Rc2Grd()

def ToolFactoryTest():
  import grit.tool.test
  return grit.tool.test.TestTool()

def ToolFactoryTranslationToTc():
  import grit.tool.transl2tc
  return grit.tool.transl2tc.TranslationToTc()

def ToolFactoryUnit():
  import grit.tool.unit
  return grit.tool.unit.UnitTestTool()


def ToolFactoryUpdateResourceIds():
  import grit.tool.update_resource_ids
  return grit.tool.update_resource_ids.UpdateResourceIds()


def ToolFactoryXmb():
  import grit.tool.xmb
  return grit.tool.xmb.OutputXmb()

def ToolAndroid2Grd():
  import grit.tool.android2grd
  return grit.tool.android2grd.Android2Grd()

# Keys for the following map
_FACTORY = 1
_REQUIRES_INPUT = 2
_HIDDEN = 3  # optional key - presence indicates tool is hidden

# Maps tool names to the tool's module.  Done as a list of (key, value) tuples
# instead of a map to preserve ordering.
_TOOLS = [
    ['android2grd', {
        _FACTORY: ToolAndroid2Grd,
        _REQUIRES_INPUT: False
    }],
    ['build', {
        _FACTORY: ToolFactoryBuild,
        _REQUIRES_INPUT: True
    }],
    ['buildinfo', {
        _FACTORY: ToolFactoryBuildInfo,
        _REQUIRES_INPUT: True
    }],
    ['count', {
        _FACTORY: ToolFactoryCount,
        _REQUIRES_INPUT: True
    }],
    [
        'menufromparts',
        {
            _FACTORY: ToolFactoryMenuTranslationsFromParts,
            _REQUIRES_INPUT: True,
            _HIDDEN: True
        }
    ],
    ['newgrd', {
        _FACTORY: ToolFactoryNewGrd,
        _REQUIRES_INPUT: False
    }],
    ['rc2grd', {
        _FACTORY: ToolFactoryRc2Grd,
        _REQUIRES_INPUT: False
    }],
    ['resize', {
        _FACTORY: ToolFactoryResizeDialog,
        _REQUIRES_INPUT: True
    }],
    ['sdiff', {
        _FACTORY: ToolFactoryDiffStructures,
        _REQUIRES_INPUT: False
    }],
    ['test', {
        _FACTORY: ToolFactoryTest,
        _REQUIRES_INPUT: True,
        _HIDDEN: True
    }],
    [
        'transl2tc',
        {
            _FACTORY: ToolFactoryTranslationToTc,
            _REQUIRES_INPUT: False
        }
    ],
    ['unit', {
        _FACTORY: ToolFactoryUnit,
        _REQUIRES_INPUT: False
    }],
    [
        'update_resource_ids',
        {
            _FACTORY: ToolFactoryUpdateResourceIds,
            _REQUIRES_INPUT: False
        }
    ],
    ['xmb', {
        _FACTORY: ToolFactoryXmb,
        _REQUIRES_INPUT: True
    }],
]


def PrintUsage():
  tool_list = ''
  for (tool, info) in _TOOLS:
    if not _HIDDEN in info:
      tool_list += '    %-12s %s\n' % (
          tool, info[_FACTORY]().ShortDescription())

  print("""GRIT - the Google Resource and Internationalization Tool

Usage: grit [GLOBALOPTIONS] TOOL [args to tool]

Global options:

  -i INPUT  Specifies the INPUT file to use (a .grd file).  If this is not
            specified, GRIT will look for the environment variable GRIT_INPUT.
            If it is not present either, GRIT will try to find an input file
            named 'resource.grd' in the current working directory.

  -h MODULE Causes GRIT to use MODULE.UnsignedFingerPrint instead of
            grit.extern.FP.UnsignedFingerprint.  MODULE must be
            available somewhere in the PYTHONPATH search path.

  -v        Print more verbose runtime information.

  -x        Print extremely verbose runtime information.  Implies -v

  -p FNAME  Specifies that GRIT should profile its execution and output the
            results to the file FNAME.

Tools:

  TOOL can be one of the following:
%s
  For more information on how to use a particular tool, and the specific
  arguments you can send to that tool, execute 'grit help TOOL'
""" % (tool_list))


class Options(object):
  """Option storage and parsing."""

  def __init__(self):
    self.hash = None
    self.input = None
    self.verbose = False
    self.extra_verbose = False
    self.output_stream = sys.stdout
    self.profile_dest = None

  def ReadOptions(self, args):
    """Reads options from the start of args and returns the remainder."""
    (opts, args) = getopt.getopt(args, 'vxi:p:h:', ('help',))
    for (key, val) in opts:
      if key == '-h': self.hash = val
      elif key == '-i': self.input = val
      elif key == '-v':
        self.verbose = True
        util.verbose = True
      elif key == '-x':
        self.verbose = True
        util.verbose = True
        self.extra_verbose = True
        util.extra_verbose = True
      elif key == '-p': self.profile_dest = val
      elif key == '--help':
        PrintUsage()
        sys.exit(0)

    if not self.input:
      if 'GRIT_INPUT' in os.environ:
        self.input = os.environ['GRIT_INPUT']
      else:
        self.input = 'resource.grd'

    return args

  def __repr__(self):
    return '(verbose: %d, input: %s)' % (
        self.verbose, self.input)


def _GetToolInfo(tool):
  """Returns the info map for the tool named 'tool' or None if there is no
  such tool."""
  matches = [t for t in _TOOLS if t[0] == tool]
  if not matches:
    return None
  else:
    return matches[0][1]


def Main(args=None):
  """Parses arguments and does the appropriate thing."""
  util.ChangeStdoutEncoding()

  # Support for setuptools console wrappers.
  if args is None:
    args = sys.argv[1:]

  options = Options()
  try:
    args = options.ReadOptions(args)  # args may be shorter after this
  except getopt.GetoptError as e:
    print("grit:", str(e))
    print("Try running 'grit help' for valid options.")
    return 1
  if not args:
    print("No tool provided.  Try running 'grit help' for a list of tools.")
    return 2

  tool = args[0]
  if tool == 'help':
    if len(args) == 1:
      PrintUsage()
      return 0
    else:
      tool = args[1]
      if not _GetToolInfo(tool):
        print("No such tool.  Try running 'grit help' for a list of tools.")
        return 2

      print("Help for 'grit %s' (for general help, run 'grit help'):\n" %
            (tool,))
      _GetToolInfo(tool)[_FACTORY]().ShowUsage()
      return 0
  if not _GetToolInfo(tool):
    print("No such tool.  Try running 'grit help' for a list of tools.")
    return 2

  try:
    if _GetToolInfo(tool)[_REQUIRES_INPUT]:
      os.stat(options.input)
  except OSError:
    print('Input file %s not found.\n'
          'To specify a different input file:\n'
          '  1. Use the GRIT_INPUT environment variable.\n'
          '  2. Use the -i command-line option.  This overrides '
          'GRIT_INPUT.\n'
          '  3. Specify neither GRIT_INPUT or -i and GRIT will try to load '
          "'resource.grd'\n"
          '     from the current directory.' % options.input)
    return 2

  if options.hash:
    grit.extern.FP.UseUnsignedFingerPrintFromModule(options.hash)

  try:
    toolobject = _GetToolInfo(tool)[_FACTORY]()
    if options.profile_dest:
      import hotshot
      prof = hotshot.Profile(options.profile_dest)
      return prof.runcall(toolobject.Run, options, args[1:])
    else:
      return toolobject.Run(options, args[1:])
  except getopt.GetoptError as e:
    print("grit: %s: %s" % (tool, str(e)))
    print("Try running 'grit help %s' for valid options." % (tool,))
    return 1


if __name__ == '__main__':
  sys.path.append(
      os.path.join(
          os.path.dirname(
              os.path.dirname(os.path.dirname(os.path.abspath(__file__)))),
          'diagnosis'))
  try:
    import crbug_1001171
    with crbug_1001171.DumpStateOnLookupError():
      sys.exit(Main(sys.argv[1:]))
  except ImportError:
    pass

  sys.exit(Main(sys.argv[1:]))
