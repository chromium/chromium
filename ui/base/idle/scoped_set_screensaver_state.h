// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IDLE_SCOPED_SET_SCREENSAVER_STATE_H_
#define UI_BASE_IDLE_SCOPED_SET_SCREENSAVER_STATE_H_

#include <optional>

namespace ui {

// RAII class to set the screensaver state for testing purposes.
// When destroyed, it restores the previous state.
class ScopedSetScreensaverState {
 public:
  explicit ScopedSetScreensaverState(bool is_screensaver_running);

  ScopedSetScreensaverState(const ScopedSetScreensaverState&) = delete;
  ScopedSetScreensaverState& operator=(const ScopedSetScreensaverState&) =
      delete;

  ~ScopedSetScreensaverState();

 private:
  std::optional<bool> previous_state_;
};

}  // namespace ui

#endif  // UI_BASE_IDLE_SCOPED_SET_SCREENSAVER_STATE_H_
