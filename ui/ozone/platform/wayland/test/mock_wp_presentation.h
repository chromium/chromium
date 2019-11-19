// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_MOCK_WP_PRESENTATION_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_MOCK_WP_PRESENTATION_H_

#include <presentation-time-server-protocol.h>

#include "base/logging.h"
#include "base/macros.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/ozone/platform/wayland/test/global_object.h"

namespace wl {

extern const struct wp_presentation_interface kMockWpPresentationImpl;

class MockWpPresentation : public GlobalObject {
 public:
  MockWpPresentation();
  ~MockWpPresentation() override;

  MOCK_METHOD2(Destroy,
               void(struct wl_client* client, struct wl_resource* resource));
  MOCK_METHOD4(Feedback,
               void(struct wl_client* client,
                    struct wl_resource* resource,
                    struct wl_resource* surface,
                    uint32_t callback));

  void set_presentation_callback(wl_resource* callback_resource) {
    DCHECK(!presentation_callback_);
    presentation_callback_ = callback_resource;
  }

  void SendPresentationCallback();

 private:
  wl_resource* presentation_callback_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(MockWpPresentation);
};

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_MOCK_WP_PRESENTATION_H_
