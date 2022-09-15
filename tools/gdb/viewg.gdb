# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Creates a SVG graph of the view hierarchy, when stopped in a
# method of an object that inherits from views::View. Requires
# graphviz.
#
# For more info see
# chromium/src/+/HEAD/docs/graphical_debugging_aid_chromium_views.md
#
# To make this command available, add the following to your ~/.gdbinit:
# source {Path to SRC Root}/tools/gdbviewg.gdb
#
# Usage: type `viewg` at the GDB prompt, given the conditions above.


define viewg
  if $argc != 0
    echo Usage: viewg
  else
    set pagination off
    set print elements 0
    set logging enabled off
    set logging file ~/state.dot
    set logging overwrite on
    set logging redirect on
    set logging enabled on
    printf "%s\n", views::PrintViewGraph(this).c_str()
    set logging enabled off
    shell dot -Tsvg -o ~/state.svg ~/state.dot
    set pagination on
  end
end
