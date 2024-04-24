// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_FLATLAND_FLATLAND_CONNECTION_H_
#define UI_OZONE_PLATFORM_FLATLAND_FLATLAND_CONNECTION_H_

#include <fuchsia/ui/composition/cpp/fidl.h>

#include <memory>
#include <string_view>

#include "base/containers/queue.h"
#include "base/functional/callback.h"
#include "base/time/time.h"

namespace ui {

// Helper class used to own fuchsia.ui.composition.Flatland to safely call
// Present. By limiting the number of Present calls, FlatlandConnection
// ensures that the Flatland will not be shut down, thus, users of
// FlatlandConnection should not call Flatland::Present on their own.
class FlatlandConnection final {
 public:
  using OnFramePresentedCallback =
      base::OnceCallback<void(base::TimeTicks actual_presentation_time,
                              base::TimeDelta future_presentation_interval)>;
  using OnErrorCallback =
      base::OnceCallback<void(fuchsia::ui::composition::FlatlandError error)>;

  explicit FlatlandConnection(std::string_view debug_name,
                              OnErrorCallback callback);
  ~FlatlandConnection();

  FlatlandConnection(const FlatlandConnection&) = delete;
  FlatlandConnection& operator=(const FlatlandConnection&) = delete;

  fuchsia::ui::composition::Flatland* flatland() { return flatland_.get(); }

  // Returns an unused, non-zero transform identifier.
  fuchsia::ui::composition::TransformId NextTransformId() {
    return {++next_transform_id_};
  }

  // Returns an unused, non-zero content identifier.
  fuchsia::ui::composition::ContentId NextContentId() {
    return {++next_content_id_};
  }

  void Present();
  void Present(fuchsia::ui::composition::PresentArgs present_args,
               OnFramePresentedCallback callback);

 private:
  // Initial fps value to calculate the future presentation interval, that is
  // used until Flatland's OnNextFrameBegin() callback is received.
  static constexpr int kInitialFramesPerSecondEstimate = 60;

  // Method that is listening to Flatland's OnNextFrameBegin() callback.
  // Returns one or more present credits.
  void OnNextFrameBegin(
      fuchsia::ui::composition::OnNextFrameBeginValues values);
  // Method that is listening to Flatland's OnFramePresented() callback.
  // Returns feedback on the prior frame presentation.
  void OnFramePresented(fuchsia::scenic::scheduling::FramePresentedInfo info);

  fuchsia::ui::composition::FlatlandPtr flatland_;
  uint64_t next_transform_id_ = 0;
  uint64_t next_content_id_ = 0;
  uint32_t present_credits_ = 1;

  // Stores the presentation interval for the future frames.
  base::TimeDelta presentation_interval_ =
      base::Hertz(kInitialFramesPerSecondEstimate);

  struct PendingPresent {
    PendingPresent(fuchsia::ui::composition::PresentArgs present_args,
                   OnFramePresentedCallback callback);
    ~PendingPresent();

    PendingPresent(PendingPresent&& other);
    PendingPresent& operator=(PendingPresent&& other);

    fuchsia::ui::composition::PresentArgs present_args;
    OnFramePresentedCallback callback;
  };

  // Keeps track of pending Presents that cannot be committed in the situation
  // when we don't have enough present credits.
  base::queue<PendingPresent> pending_presents_;
  // Keeps track of release fences for the previous frame, indicating when it
  // is safe to reuse the resources. Ozone defines and sends release fences
  // for the current frame, whereas Flatland expects the release fences for
  // the previous frame resources.
  std::vector<zx::event> previous_present_release_fences_;
  // Keeps track of Presents that are committed, but Flatland hasn't indicated
  // that have taken effect by calling OnFramePresented().
  base::queue<OnFramePresentedCallback> presented_callbacks_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_FLATLAND_FLATLAND_CONNECTION_H_
