// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/mock_wp_presentation.h"

#include <wayland-server-core.h>

#include "base/logging.h"
#include "ui/ozone/platform/wayland/test/server_object.h"

namespace wl {

namespace {

void Feedback(struct wl_client* client,
              struct wl_resource* resource,
              struct wl_resource* surface,
              uint32_t callback) {
  auto* wp_presentation = GetUserDataAs<MockWpPresentation>(resource);
  wl_resource* presentation_feedback_resource =
      wl_resource_create(client, &wp_presentation_feedback_interface,
                         wl_resource_get_version(resource), callback);
  DCHECK(presentation_feedback_resource);
  wp_presentation->OnFeedback(presentation_feedback_resource);
  wp_presentation->Feedback(client, resource, surface, callback);
}

}  // namespace

const struct wp_presentation_interface kMockWpPresentationImpl = {
    &DestroyResource,  // destroy
    &Feedback,         // feedback
};

MockWpPresentation::MockWpPresentation()
    : GlobalObject(&wp_presentation_interface, &kMockWpPresentationImpl, 1) {}

MockWpPresentation::~MockWpPresentation() = default;

void MockWpPresentation::OnFeedback(wl_resource* callback_resource) {
  DCHECK(callback_resource);
  presentation_callbacks_.emplace_back(callback_resource);
}

void MockWpPresentation::DropPresentationCallback(bool last) {
  wl_resource* callback_resource = GetPresentationCallbackResource(last);
  wl_resource_destroy(callback_resource);
}

void MockWpPresentation::SendPresentationCallback() {
  SendPresentationFeedbackToClient(/*last=*/false, /*discarded=*/false);
}

void MockWpPresentation::SendPresentationCallbackDiscarded(bool last) {
  SendPresentationFeedbackToClient(last, /*discarded=*/true);
}

void MockWpPresentation::SendPresentationFeedbackToClient(bool last,
                                                          bool discarded) {
  wl_resource* callback_resource = GetPresentationCallbackResource(last);
  if (!callback_resource)
    return;

  if (discarded) {
    wp_presentation_feedback_send_discarded(callback_resource);
  } else {
    // TODO(msisov): add support for test provided presentation feedback values.
    wp_presentation_feedback_send_presented(
        callback_resource, 0 /* tv_sec_hi */, 0 /* tv_sec_lo */,
        0 /* tv_nsec */, 0 /* refresh */, 0 /* seq_hi */, 0 /* seq_lo */,
        0 /* flags */);
  }
  wl_client_flush(wl_resource_get_client(callback_resource));
  wl_resource_destroy(callback_resource);
}

wl_resource* MockWpPresentation::GetPresentationCallbackResource(bool last) {
  if (presentation_callbacks_.empty()) {
    LOG(WARNING) << "MockWpPresentation doesn't have pending requests for "
                    "presentation feedbacks.";
    return nullptr;
  }

  wl_resource* callback_resource = nullptr;
  if (last) {
    callback_resource = presentation_callbacks_.back();
    presentation_callbacks_.pop_back();
  } else {
    callback_resource = presentation_callbacks_.front();
    presentation_callbacks_.erase(presentation_callbacks_.begin());
  }
  return callback_resource;
}

}  // namespace wl
