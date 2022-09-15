// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_CLANG_PLUGINS_OPTIONS_H_
#define TOOLS_CLANG_PLUGINS_OPTIONS_H_

namespace chrome_checker {

struct Options {
  bool check_base_classes = false;
  bool check_ipc = false;
  bool check_layout_object_methods = false;
  bool raw_ref_template_as_trivial_member = false;
};

}  // namespace chrome_checker

#endif  // TOOLS_CLANG_PLUGINS_OPTIONS_H_
