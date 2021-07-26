// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_FLATLAND_FLATLAND_CONNECTION_H_
#define UI_OZONE_PLATFORM_FLATLAND_FLATLAND_CONNECTION_H_

#include <fuchsia/ui/composition/cpp/fidl.h>

namespace ui {

// Helper class used to own fuchsia.ui.composition.Flatland to safely call
// Present. By limiting the number of Present calls, FlatlandConnection ensures
// that the Session will not be shut down, thus, users of FlatlandConnection
// should not call Present on their own.
//
// More information can be found in the fuchsia.flatland.scheduling FIDL
// library, in the prediction_info.fidl file.
class FlatlandConnection {
 public:
  FlatlandConnection();
  ~FlatlandConnection();

  FlatlandConnection(const FlatlandConnection&) = delete;
  FlatlandConnection& operator=(const FlatlandConnection&) = delete;

  void QueuePresent();

  fuchsia::ui::composition::Flatland* flatland() { return flatland_.get(); }

 private:
  void QueuePresentHelper();
  void OnFramePresented(fuchsia::scenic::scheduling::FramePresentedInfo info);

  fuchsia::ui::composition::FlatlandPtr flatland_;

  bool presents_allowed_ = true;
  bool present_queued_ = false;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_FLATLAND_FLATLAND_CONNECTION_H_
