// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_TEST_DISPLAY_TEST_UTIL_H_
#define UI_DISPLAY_TEST_DISPLAY_TEST_UTIL_H_

#include <stdint.h>

#include <string>
#include <utility>
#include <vector>

#include "ui/display/display.h"
#include "ui/display/util/display_util.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ui/display/manager/managed_display_info.h"
#endif

namespace display {

// Use this class instead of calling `SetInternalDisplayIds()` in unit tests to
// avoid leaking the state to other tests.
class ScopedSetInternalDisplayIds {
 public:
  explicit ScopedSetInternalDisplayIds(base::flat_set<int64_t> ids) {
    SetInternalDisplayIds(std::move(ids));
  }
  explicit ScopedSetInternalDisplayIds(int64_t id) {
    SetInternalDisplayIds({id});
  }
  ScopedSetInternalDisplayIds(const ScopedSetInternalDisplayIds&) = delete;
  ScopedSetInternalDisplayIds operator=(const ScopedSetInternalDisplayIds&) =
      delete;
  ~ScopedSetInternalDisplayIds() { SetInternalDisplayIds({}); }
};

inline void PrintTo(const Display& display, ::std::ostream* os) {
  *os << display.ToString();
}

#if BUILDFLAG(IS_CHROMEOS)
// Create a list of ManagedDisplayInfo from the string specs.
// If a list of existing displays are present, the function will
// reuse the display ID.
DISPLAY_EXPORT std::vector<ManagedDisplayInfo> CreateDisplayInfoListFromSpecs(
    const std::string& display_specs,
    const std::vector<Display>& existing_displays,
    bool generate_new_ids);
#endif
}  // namespace display

#endif  // UI_DISPLAY_TEST_DISPLAY_TEST_UTIL_H_
