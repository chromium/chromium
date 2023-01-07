// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_ZAURA_POPUP_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_ZAURA_POPUP_H_

#include <aura-shell-server-protocol.h>

#include "ui/ozone/platform/wayland/test/server_object.h"

namespace wl {

extern const struct zaura_popup_interface kTestZAuraPopupImpl;

// Manages zaura_popup object.
class TestZAuraPopup : public ServerObject {
 public:
  explicit TestZAuraPopup(wl_resource* resource);

  TestZAuraPopup(const TestZAuraPopup&) = delete;
  TestZAuraPopup& operator=(const TestZAuraPopup&) = delete;

  ~TestZAuraPopup() override;
};

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_ZAURA_POPUP_H_
