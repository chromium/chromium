// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_CLANG_PLUGINS_OPTIONS_H_
#define TOOLS_CLANG_PLUGINS_OPTIONS_H_

#include <string>
#include <vector>

namespace chrome_checker {

struct Options {
  bool check_base_classes = false;
  bool check_blink_data_member_type = false;
  bool check_ipc = false;
  bool check_layout_object_methods = false;
  bool raw_ref_template_as_trivial_member = false;
  bool check_bad_raw_ptr_cast = false;
  bool check_raw_ptr_fields = false;
  bool check_stack_allocated = false;
  bool check_raw_ref_fields = false;
  std::string exclude_fields_file;
  std::vector<std::string> raw_ptr_paths_to_exclude_lines;
};

}  // namespace chrome_checker

#endif  // TOOLS_CLANG_PLUGINS_OPTIONS_H_
