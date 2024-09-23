// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_DATA_DEVICE_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_DATA_DEVICE_H_

#include <wayland-server-protocol.h>

#include <cstdint>

#include "base/memory/raw_ptr.h"
#include "ui/ozone/platform/wayland/test/mock_surface.h"
#include "ui/ozone/platform/wayland/test/test_selection_device_manager.h"

struct wl_resource;

namespace wl {

extern const struct wl_data_device_interface kTestDataDeviceImpl;

class TestDataOffer;
class TestDataSource;
class TestDataDeviceManager;

class TestDataDevice : public TestSelectionDevice {
 public:
  TestDataDevice(wl_resource* resource, TestDataDeviceManager* manager);

  TestDataDevice(const TestDataDevice&) = delete;
  TestDataDevice& operator=(const TestDataDevice&) = delete;

  ~TestDataDevice() override;

  TestDataOffer* CreateAndSendDataOffer();
  void SetSelection(TestDataSource* data_source, uint32_t serial);
  void StartDrag(TestDataSource* data_source,
                 MockSurface* origin,
                 uint32_t serial);
  void SendOfferAndEnter(MockSurface* origin, const gfx::Point& location);

  void OnEnter(uint32_t serial,
               wl_resource* surface,
               wl_fixed_t x,
               wl_fixed_t y,
               TestDataOffer* data_offer);
  void OnLeave();
  void OnMotion(uint32_t time, wl_fixed_t x, wl_fixed_t y);
  void OnDrop();

  uint32_t drag_serial() const { return drag_serial_; }

  // Configure this data device to not send offer/enter events next time it
  // receives a start_drag request. Useful for tests that wish to emulate
  // some specific compositor behavior when starting a drag session.
  void disable_auto_send_start_drag_events() {
    auto_send_start_drag_events_ = false;
  }

 private:
  const raw_ptr<TestDataDeviceManager> manager_;

  uint32_t drag_serial_ = 0;
  bool auto_send_start_drag_events_ = true;
};

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_DATA_DEVICE_H_
