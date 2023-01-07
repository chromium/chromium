// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_FLATLAND_TESTS_FAKE_VIEW_REF_FOCUSED_H_
#define UI_OZONE_PLATFORM_FLATLAND_TESTS_FAKE_VIEW_REF_FOCUSED_H_

#include <fuchsia/ui/views/cpp/fidl.h>

using ViewRefFocused = fuchsia::ui::views::ViewRefFocused;

namespace ui {

// This fixture allows the test to control server-side behavior of
// fuchsia.ui.views.ViewRefFocused protocol.
class FakeViewRefFocused : public ViewRefFocused {
 public:
  FakeViewRefFocused();
  ~FakeViewRefFocused() override;

  using WatchCallback = ViewRefFocused::WatchCallback;
  void Watch(WatchCallback callback) override;
  void ScheduleCallback(bool focused);

  std::size_t times_watched() const;

 private:
  std::size_t times_watched_ = 0;
  WatchCallback callback_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_FLATLAND_TESTS_FAKE_VIEW_REF_FOCUSED_H_
