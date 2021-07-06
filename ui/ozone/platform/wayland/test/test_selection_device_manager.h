// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_SELECTION_DEVICE_MANAGER_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_SELECTION_DEVICE_MANAGER_H_

#include <primary-selection-unstable-v1-server-protocol.h>

#include <cstdint>

#include "ui/ozone/platform/wayland/test/global_object.h"
#include "ui/ozone/platform/wayland/test/server_object.h"
#include "ui/ozone/public/platform_clipboard.h"

namespace wl {

class TestSelectionSource;
class TestSelectionDevice;

// Manage wl_data_device_manager object.
class TestSelectionDeviceManager : public GlobalObject {
 public:
  TestSelectionDeviceManager();
  ~TestSelectionDeviceManager() override;

  TestSelectionDeviceManager(const TestSelectionDeviceManager&) = delete;
  TestSelectionDeviceManager& operator=(const TestSelectionDeviceManager&) =
      delete;

  const TestSelectionDevice* device() const { return device_; }
  const TestSelectionSource* source() const { return source_; }

 private:
  // Protocol object requests:
  static void CreateSource(wl_client* client,
                           wl_resource* manager_resource,
                           uint32_t id);
  static void GetDevice(wl_client* client,
                        wl_resource* manager_resource,
                        uint32_t id,
                        wl_resource* seat_resource);

  TestSelectionDevice* device_ = nullptr;
  TestSelectionSource* source_ = nullptr;

  static const struct zwp_primary_selection_device_manager_v1_interface
      kTestSelectionManagerImpl;
};

class TestSelectionSource : public ServerObject {
 public:
  explicit TestSelectionSource(wl_resource* resource);
  ~TestSelectionSource() override;

 private:
  friend class TestSelectionDeviceManager;

  // Protocol object requests:
  static void Offer(struct wl_client* client,
                    struct wl_resource* resource,
                    const char* mime_type);
};

class TestSelectionDevice : public ServerObject {
 public:
  TestSelectionDevice(wl_resource* resource, wl_client* client);
  ~TestSelectionDevice() override;

  void SendSelectionOffer(const ui::PlatformClipboard::DataMap& data_map);

 private:
  friend class TestSelectionDeviceManager;

  // Protocol object requests:
  static void SetSelection(struct wl_client* client,
                           struct wl_resource* resource,
                           struct wl_resource* source,
                           uint32_t serial);
};

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_SELECTION_DEVICE_MANAGER_H_
