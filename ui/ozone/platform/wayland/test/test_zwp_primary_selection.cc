// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/test_zwp_primary_selection.h"

#include <primary-selection-unstable-v1-server-protocol.h>
#include <wayland-server-core.h>

#include <cstdint>

#include "base/notreached.h"
#include "ui/ozone/platform/wayland/test/test_selection_device_manager.h"

// TODO(crbug.com/1204670): Implement zwp primary selection support.

namespace wl {

namespace {

void Destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

struct ZwpPrimarySelectionOffer : public TestSelectionOffer::Delegate {
  void SendOffer(const std::string& mime_type,
                 ui::PlatformClipboard::Data data) override {
    NOTIMPLEMENTED();
  }

  void OnDestroying() override { NOTIMPLEMENTED(); }
};

struct ZwpPrimarySelectionDevice : public TestSelectionDevice::Delegate {
  TestSelectionOffer* CreateAndSendOffer() override {
    NOTIMPLEMENTED();
    return nullptr;
  }

  void SendSelection(TestSelectionOffer* offer) override { NOTIMPLEMENTED(); }

  void HandleSetSelection(TestSelectionSource* source,
                          uint32_t serial) override {
    NOTIMPLEMENTED();
  }

  void OnDestroying() override { NOTIMPLEMENTED(); }
};

struct ZwpPrimarySelectionSource : public TestSelectionSource::Delegate {
  void HandleOffer(const std::string& mime_type) override { NOTIMPLEMENTED(); }

  void OnDestroying() override { NOTIMPLEMENTED(); }
};

struct ZwpPrimarySelectionDeviceManager
    : public TestSelectionDeviceManager::Delegate {
  explicit ZwpPrimarySelectionDeviceManager(uint32_t version)
      : version_(version) {}
  ~ZwpPrimarySelectionDeviceManager() override = default;

  TestSelectionDevice* CreateDevice(wl_client* client, uint32_t id) override {
    const struct zwp_primary_selection_device_v1_interface
        kTestSelectionDeviceImpl = {&TestSelectionDevice::SetSelection,
                                    &Destroy};
    wl_resource* resource = CreateResourceWithImpl<TestSelectionDevice>(
        client, &zwp_primary_selection_device_v1_interface, version_,
        &kTestSelectionDeviceImpl, id, new ZwpPrimarySelectionDevice);
    return GetUserDataAs<TestSelectionDevice>(resource);
  }

  TestSelectionSource* CreateSource(wl_client* client, uint32_t id) override {
    const struct zwp_primary_selection_source_v1_interface
        kTestSelectionSourceImpl = {&TestSelectionSource::Offer, &Destroy};
    wl_resource* resource = CreateResourceWithImpl<TestSelectionSource>(
        client, &zwp_primary_selection_source_v1_interface, version_,
        &kTestSelectionSourceImpl, id, new ZwpPrimarySelectionSource);
    return GetUserDataAs<TestSelectionSource>(resource);
  }

  void OnDestroying() override { delete this; }

 private:
  const uint32_t version_;
};

}  // namespace

TestSelectionDeviceManager* CreateTestSelectionManagerZwp() {
  constexpr uint32_t kVersion = 1;
  const struct zwp_primary_selection_device_manager_v1_interface
      kTestSelectionManagerImpl = {&TestSelectionDeviceManager::CreateSource,
                                   &TestSelectionDeviceManager::GetDevice,
                                   &Destroy};
  const TestSelectionDeviceManager::InterfaceInfo interface_info = {
      .interface = &zwp_primary_selection_device_manager_v1_interface,
      .implementation = &kTestSelectionManagerImpl,
      .version = kVersion};
  return new TestSelectionDeviceManager(
      interface_info, new ZwpPrimarySelectionDeviceManager(kVersion));
}

}  // namespace wl
