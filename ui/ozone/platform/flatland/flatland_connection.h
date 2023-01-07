// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_FLATLAND_FLATLAND_CONNECTION_H_
#define UI_OZONE_PLATFORM_FLATLAND_FLATLAND_CONNECTION_H_

#include <fuchsia/ui/composition/cpp/fidl.h>

#include <memory>

#include "base/callback.h"
#include "base/containers/queue.h"

namespace ui {

// Helper class used to own fuchsia.ui.composition.Flatland to safely call
// Present. By limiting the number of Present calls, FlatlandConnection ensures
// that the Flatland will not be shut down, thus, users of FlatlandConnection
// should not call Flatland::Present on their own.
class FlatlandConnection final {
 public:
  explicit FlatlandConnection(const std::string& debug_name);
  ~FlatlandConnection();

  FlatlandConnection(const FlatlandConnection&) = delete;
  FlatlandConnection& operator=(const FlatlandConnection&) = delete;

  fuchsia::ui::composition::Flatland* flatland() { return flatland_.get(); }

  fuchsia::ui::composition::TransformId NextTransformId() {
    return {++next_transform_id_};
  }

  fuchsia::ui::composition::ContentId NextContentId() {
    return {++next_content_id_};
  }

  using OnFramePresentedCallback =
      base::OnceCallback<void(zx_time_t actual_presentation_time)>;
  void Present();
  void Present(fuchsia::ui::composition::PresentArgs present_args,
               OnFramePresentedCallback callback);

 private:
  void OnError(fuchsia::ui::composition::FlatlandError error);
  void OnNextFrameBegin(
      fuchsia::ui::composition::OnNextFrameBeginValues values);
  void OnFramePresented(fuchsia::scenic::scheduling::FramePresentedInfo info);

  fuchsia::ui::composition::FlatlandPtr flatland_;
  uint64_t next_transform_id_ = 0;
  uint64_t next_content_id_ = 0;
  uint32_t present_credits_ = 1;

  struct PendingPresent {
    PendingPresent(fuchsia::ui::composition::PresentArgs present_args,
                   OnFramePresentedCallback callback);
    ~PendingPresent();

    PendingPresent(PendingPresent&& other);
    PendingPresent& operator=(PendingPresent&& other);

    fuchsia::ui::composition::PresentArgs present_args;
    OnFramePresentedCallback callback;
  };
  base::queue<PendingPresent> pending_presents_;
  std::vector<zx::event> previous_present_release_fences_;
  base::queue<OnFramePresentedCallback> presented_callbacks_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_FLATLAND_FLATLAND_CONNECTION_H_
