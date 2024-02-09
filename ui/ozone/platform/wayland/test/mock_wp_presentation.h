// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_MOCK_WP_PRESENTATION_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_MOCK_WP_PRESENTATION_H_

#include <presentation-time-server-protocol.h>

#include <optional>

#include "base/check.h"
#include "base/memory/raw_ptr.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/ozone/platform/wayland/test/global_object.h"

namespace wl {

extern const struct wp_presentation_interface kMockWpPresentationImpl;

class MockWpPresentation : public GlobalObject {
 public:
  struct PresentationFeedbackParams {
    uint32_t tv_sec_hi;
    uint32_t tv_sec_lo;
    uint32_t tv_nsec;
    uint32_t refresh;
    uint32_t seq_hi;
    uint32_t seq_lo;
    uint32_t flags;
  };

  MockWpPresentation();

  MockWpPresentation(const MockWpPresentation&) = delete;
  MockWpPresentation& operator=(const MockWpPresentation&) = delete;

  ~MockWpPresentation() override;

  MOCK_METHOD2(Destroy,
               void(struct wl_client* client, struct wl_resource* resource));
  MOCK_METHOD4(Feedback,
               void(struct wl_client* client,
                    struct wl_resource* resource,
                    struct wl_resource* surface,
                    uint32_t callback));

  size_t num_of_presentation_callbacks() const {
    return presentation_callbacks_.size();
  }

  void OnFeedback(wl_resource* callback_resource);

  // Drops first presentation callback from |presentation_callbacks|. If |last|
  // is true, the last item is dropped instead.
  void DropPresentationCallback(bool last = false);

  // Sends successful presentation callback for the first callback item in
  // |presentation_callbacks| and deletes that.
  void SendPresentationCallback();

  // Sends discarded presentation callback for the first callback item (if
  // |last| is true, then the very recent one) in |presentation_callbacks| and
  // deletes that.
  void SendPresentationCallbackDiscarded(bool last = false);

  // Sends either discarded or feedback with `params` to client and deletes
  // the feedback resource. If `params` is null, then send discarded feedback.
  // Which feedback is sent (the oldest or the most recent) is based on |last|
  // value.
  void SendPresentationFeedbackToClient(
      bool last,
      std::optional<PresentationFeedbackParams> params);

 private:
  // Sends either discarded or succeeded, which is based on |discarded|,
  // feedback to client and deletes the feedback resource. Which feedback is
  // sent (the oldest or the most recent) is based on |last| value.
  void SendPresentationFeedbackToClient(bool last, bool discarded);

  wl_resource* GetPresentationCallbackResource(bool last);

  std::vector<raw_ptr<wl_resource>> presentation_callbacks_;
};

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_MOCK_WP_PRESENTATION_H_
