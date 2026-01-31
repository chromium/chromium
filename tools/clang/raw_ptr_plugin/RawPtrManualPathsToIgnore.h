// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_CLANG_RAW_PTR_PLUGIN_RAWPTRMANUALPATHSTOIGNORE_H_
#define TOOLS_CLANG_RAW_PTR_PLUGIN_RAWPTRMANUALPATHSTOIGNORE_H_

#include <string>
#include <vector>

namespace raw_ptr_plugin {

// TODO(crbug.com/330759291): when Clang rolls and we know it's safe to do so,
// remove `mix_in_legacy_oilpanized_paths` (and the associated options
// plumbing).
void AddManualPathsToIgnore(std::vector<std::string>& paths,
                            bool add_legacy_oilpanized_paths = true);

}  // namespace raw_ptr_plugin
#endif  // TOOLS_CLANG_RAW_PTR_PLUGIN_RAWPTRMANUALPATHSTOIGNORE_H_
