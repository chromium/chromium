// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_DATA_OFFER_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_DATA_OFFER_H_

#include <wayland-server-protocol.h>

#include "ui/ozone/platform/wayland/test/test_selection_device_manager.h"

struct wl_resource;

namespace wl {

extern const struct wl_data_offer_interface kTestDataOfferImpl;

class TestDataOffer : public TestSelectionOffer {
 public:
  explicit TestDataOffer(wl_resource* resource);

  TestDataOffer(const TestDataOffer&) = delete;
  TestDataOffer& operator=(const TestDataOffer&) = delete;

  ~TestDataOffer() override;

  static TestDataOffer* FromResource(wl_resource* resource);

  void SetActions(uint32_t dnd_actions, uint32_t preferred_action);

  void OnSourceActions(uint32_t source_actions);
  void OnAction(uint32_t dnd_action);

  uint32_t supported_actions() const { return client_supported_actions_; }
  uint32_t preferred_action() const { return client_preferred_action_; }

 private:
  uint32_t client_supported_actions_ = WL_DATA_DEVICE_MANAGER_DND_ACTION_NONE;
  uint32_t client_preferred_action_ = WL_DATA_DEVICE_MANAGER_DND_ACTION_NONE;
};

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_DATA_OFFER_H_
