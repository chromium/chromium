# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
    Creates a SVG graph of the view hierarchy, when stopped in a
    method of an object that inherits from views::View. Requires
    graphviz.

    For more info see
    chromium/src/+/HEAD/docs/graphical_debugging_aid_chromium_views.md

    To make this command available, add the following to your ~/.lldbinit:
    command script import {Path to SRC Root}/tools/lldb/lldb_viewg.py

    Usage: type `viewg` at the LLDB prompt, given the conditions above.
"""

import lldb
import os

def view_graph(debugger, command, result, internal_dict):
  dot = os.path.expanduser("~/state.dot")
  svg = os.path.splitext(dot)[0] + ".svg"

  process = debugger.GetSelectedTarget().GetProcess()
  frame = process.GetSelectedThread().GetSelectedFrame()

  cstr_address = frame.EvaluateExpression("views::PrintViewGraph(this).c_str()")
  if not cstr_address.GetError().Success():
    return
  str_size = frame.EvaluateExpression("(size_t)strlen(%s)"
                                      % cstr_address.GetValue())
  if not str_size.GetError().Success():
    return
  error = lldb.SBError()
  dot_str = process.ReadCStringFromMemory(int(cstr_address.GetValue(), 16),
                                          str_size.GetValueAsUnsigned(), error)
  if not error.Success():
    return

  with open(dot, "w") as f:
    f.write(dot_str)
    os.system("dot -Tsvg -o %s %s" % (svg, dot))

def __lldb_init_module(debugger, internal_dict):
  debugger.HandleCommand('command script add -f lldb_viewg.view_graph viewg')
