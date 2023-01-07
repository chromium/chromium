// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_DATA_SOURCE_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_DATA_SOURCE_H_

#include <wayland-server-protocol.h>

#include "base/memory/weak_ptr.h"
#include "ui/ozone/platform/wayland/test/test_selection_device_manager.h"

struct wl_resource;

namespace wl {

extern const struct wl_data_source_interface kTestDataSourceImpl;

class TestDataSource : public TestSelectionSource {
 public:
  explicit TestDataSource(wl_resource* resource);

  TestDataSource(const TestDataSource&) = delete;
  TestDataSource& operator=(const TestDataSource&) = delete;

  ~TestDataSource() override;

  void SetActions(uint32_t dnd_actions);

  uint32_t actions() const { return actions_; }

 private:
  uint32_t actions_ = WL_DATA_DEVICE_MANAGER_DND_ACTION_NONE;
};

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_DATA_SOURCE_H_
