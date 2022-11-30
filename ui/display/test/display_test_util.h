// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_TEST_DISPLAY_TEST_UTIL_H_
#define UI_DISPLAY_TEST_DISPLAY_TEST_UTIL_H_

#include "ui/display/display.h"
#include "ui/display/util/display_util.h"

namespace display {

// Use this class instead of calling `SetInternalDisplayIds()` in unit tests to
// avoid leaking the state to other tests.
class ScopedSetInternalDisplayIds {
 public:
  explicit ScopedSetInternalDisplayIds(const base::flat_set<int64_t> ids) {
    SetInternalDisplayIds(ids);
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

}  // namespace display

#endif  // UI_DISPLAY_TEST_DISPLAY_TEST_UTIL_H_
