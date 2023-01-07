#!/bin/sh

# Copyright 2009 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

system_root=`cygpath "$SYSTEMROOT"`
export PATH="/usr/bin:$system_root/system32:$system_root:$system_root/system32/WBEM"

exec_dir=$(dirname $0)

"$exec_dir/../../../third_party/python_24/python.exe" \
    "$exec_dir/extract_actions.py" "$@"
