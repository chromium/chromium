// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/test_zwp_primary_selection.h"

#include <primary-selection-unstable-v1-server-protocol.h>
#include <wayland-server-core.h>

#include <cstdint>
#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "ui/ozone/platform/wayland/test/test_selection_device_manager.h"

// ZwpPrimarySelection* classes contain protocol-specific implementation of
// TestSelection*::Delegate interfaces, such that primary selection test
// cases may be set-up and run against test wayland compositor.

namespace wl {

namespace {

void Destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

struct ZwpPrimarySelectionOffer final : public TestSelectionOffer::Delegate {
  void SendOffer(const std::string& mime_type) override {
    zwp_primary_selection_offer_v1_send_offer(offer->resource(),
                                              mime_type.c_str());
  }

  raw_ptr<TestSelectionOffer> offer = nullptr;
};

struct ZwpPrimarySelectionDevice final : public TestSelectionDevice::Delegate {
  TestSelectionOffer* CreateAndSendOffer() override {
    static const struct zwp_primary_selection_offer_v1_interface kOfferImpl = {
        &TestSelectionOffer::Receive, &Destroy};
    wl_resource* device_resource = device->resource();
    const int version = wl_resource_get_version(device_resource);
    auto owned_delegate = std::make_unique<ZwpPrimarySelectionOffer>();
    auto* delegate = owned_delegate.get();
    wl_resource* new_offer_resource =
        CreateResourceWithImpl<TestSelectionOffer>(
            wl_resource_get_client(device->resource()),
            &zwp_primary_selection_offer_v1_interface, version, &kOfferImpl, 0,
            std::move(owned_delegate));
    delegate->offer = GetUserDataAs<TestSelectionOffer>(new_offer_resource);
    zwp_primary_selection_device_v1_send_data_offer(device_resource,
                                                    new_offer_resource);
    return delegate->offer;
  }

  void SendSelection(TestSelectionOffer* offer) override {
    CHECK(offer);
    zwp_primary_selection_device_v1_send_selection(device->resource(),
                                                   offer->resource());
  }

  void HandleSetSelection(TestSelectionSource* source,
                          uint32_t serial) override {
    NOTIMPLEMENTED();
  }

  raw_ptr<TestSelectionDevice> device = nullptr;
};

struct ZwpPrimarySelectionSource : public TestSelectionSource::Delegate {
  void SendSend(const std::string& mime_type,
                base::ScopedFD write_fd) override {
    zwp_primary_selection_source_v1_send_send(
        source->resource(), mime_type.c_str(), write_fd.get());
    wl_client_flush(wl_resource_get_client(source->resource()));
  }

  void SendFinished() override {
    NOTREACHED_IN_MIGRATION() << "The interface does not support this method.";
  }

  void SendCancelled() override {
    zwp_primary_selection_source_v1_send_cancelled(source->resource());
  }

  void SendDndAction(uint32_t action) override {
    NOTREACHED_IN_MIGRATION() << "The interface does not support this method.";
  }

  void SendDndDropPerformed() override {
    NOTREACHED_IN_MIGRATION() << "The interface does not support this method.";
  }

  raw_ptr<TestSelectionSource> source = nullptr;
};

struct ZwpPrimarySelectionDeviceManager
    : public TestSelectionDeviceManager::Delegate {
  explicit ZwpPrimarySelectionDeviceManager(uint32_t version)
      : version_(version) {}
  ~ZwpPrimarySelectionDeviceManager() override = default;

  TestSelectionDevice* CreateDevice(wl_client* client, uint32_t id) override {
    static const struct zwp_primary_selection_device_v1_interface
        kTestSelectionDeviceImpl = {&TestSelectionDevice::SetSelection,
                                    &Destroy};
    auto owned_delegate = std::make_unique<ZwpPrimarySelectionDevice>();
    auto* delegate = owned_delegate.get();
    wl_resource* resource = CreateResourceWithImpl<TestSelectionDevice>(
        client, &zwp_primary_selection_device_v1_interface, version_,
        &kTestSelectionDeviceImpl, id, std::move(owned_delegate));
    delegate->device = GetUserDataAs<TestSelectionDevice>(resource);
    return delegate->device;
  }

  TestSelectionSource* CreateSource(wl_client* client, uint32_t id) override {
    static const struct zwp_primary_selection_source_v1_interface
        kTestSelectionSourceImpl = {&TestSelectionSource::Offer, &Destroy};
    auto owned_delegate = std::make_unique<ZwpPrimarySelectionSource>();
    auto* delegate = owned_delegate.get();
    wl_resource* resource = CreateResourceWithImpl<TestSelectionSource>(
        client, &zwp_primary_selection_source_v1_interface, version_,
        &kTestSelectionSourceImpl, id, std::move(owned_delegate));
    delegate->source = GetUserDataAs<TestSelectionSource>(resource);
    return delegate->source;
  }

 private:
  const uint32_t version_;
};

}  // namespace

std::unique_ptr<TestSelectionDeviceManager> CreateTestSelectionManagerZwp() {
  constexpr uint32_t kVersion = 1;
  static const struct zwp_primary_selection_device_manager_v1_interface
      kTestSelectionManagerImpl = {&TestSelectionDeviceManager::CreateSource,
                                   &TestSelectionDeviceManager::GetDevice,
                                   &Destroy};
  static const TestSelectionDeviceManager::InterfaceInfo interface_info = {
      .interface = &zwp_primary_selection_device_manager_v1_interface,
      .implementation = &kTestSelectionManagerImpl,
      .version = kVersion};
  return std::make_unique<TestSelectionDeviceManager>(
      interface_info,
      std::make_unique<ZwpPrimarySelectionDeviceManager>(kVersion));
}

}  // namespace wl
