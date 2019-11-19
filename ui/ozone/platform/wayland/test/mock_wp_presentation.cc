// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/mock_wp_presentation.h"

#include <wayland-server-core.h>

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
  wp_presentation->set_presentation_callback(presentation_feedback_resource);
  wp_presentation->Feedback(client, resource, surface, callback);
}

}  // namespace

const struct wp_presentation_interface kMockWpPresentationImpl = {
    &DestroyResource,  // destroy
    &Feedback,         // feedback
};

MockWpPresentation::MockWpPresentation()
    : GlobalObject(&wp_presentation_interface, &kMockWpPresentationImpl, 1) {}

MockWpPresentation::~MockWpPresentation() {}

void MockWpPresentation::SendPresentationCallback() {
  if (!presentation_callback_)
    return;

  // TODO(msisov): add support for test provided presentation feedback values.
  wp_presentation_feedback_send_presented(
      presentation_callback_, 0 /* tv_sec_hi */, 0 /* tv_sec_lo */,
      0 /* tv_nsec */, 0 /* refresh */, 0 /* seq_hi */, 0 /* seq_lo */,
      0 /* flags */);
  wl_client_flush(wl_resource_get_client(presentation_callback_));
  wl_resource_destroy(presentation_callback_);
  presentation_callback_ = nullptr;
}

}  // namespace wl
