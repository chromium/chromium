// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/test_gtk_primary_selection.h"

#include <gtk-primary-selection-server-protocol.h>
#include <wayland-server-core.h>

#include <cstdint>
#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "ui/ozone/platform/wayland/test/test_selection_device_manager.h"
#include "ui/ozone/platform/wayland/test/test_wayland_server_thread.h"

// GtkPrimarySelection* classes contain protocol-specific implementation of
// TestSelection*::Delegate interfaces, such that primary selection test
// cases may be set-up and run against test wayland compositor.

namespace wl {

namespace {

void Destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

struct GtkPrimarySelectionOffer final : public TestSelectionOffer::Delegate {
  void SendOffer(const std::string& mime_type) override {
    gtk_primary_selection_offer_send_offer(offer->resource(),
                                           mime_type.c_str());
  }

  raw_ptr<TestSelectionOffer> offer = nullptr;
};

struct GtkPrimarySelectionDevice final : public TestSelectionDevice::Delegate {
  TestSelectionOffer* CreateAndSendOffer() override {
    static const struct gtk_primary_selection_offer_interface kOfferImpl = {
        &TestSelectionOffer::Receive, &Destroy};
    wl_resource* device_resource = device->resource();
    const int version = wl_resource_get_version(device_resource);
    auto owned_delegate = std::make_unique<GtkPrimarySelectionOffer>();
    auto* delegate = owned_delegate.get();
    wl_resource* new_offer_resource =
        CreateResourceWithImpl<TestSelectionOffer>(
            wl_resource_get_client(device->resource()),
            &gtk_primary_selection_offer_interface, version, &kOfferImpl, 0,
            std::move(owned_delegate));
    delegate->offer = GetUserDataAs<TestSelectionOffer>(new_offer_resource);
    gtk_primary_selection_device_send_data_offer(device_resource,
                                                 new_offer_resource);
    return delegate->offer;
  }

  void SendSelection(TestSelectionOffer* offer) override {
    CHECK(offer);
    gtk_primary_selection_device_send_selection(device->resource(),
                                                offer->resource());
  }

  void HandleSetSelection(TestSelectionSource* source,
                          uint32_t serial) override {
    NOTIMPLEMENTED();
  }

  raw_ptr<TestSelectionDevice> device = nullptr;
};

struct GtkPrimarySelectionSource : public TestSelectionSource::Delegate {
  void SendSend(const std::string& mime_type,
                base::ScopedFD write_fd) override {
    gtk_primary_selection_source_send_send(source->resource(),
                                           mime_type.c_str(), write_fd.get());
    wl_client_flush(wl_resource_get_client(source->resource()));
  }

  void SendFinished() override {
    NOTREACHED_IN_MIGRATION() << "The interface does not support this method.";
  }

  void SendCancelled() override {
    gtk_primary_selection_source_send_cancelled(source->resource());
  }

  void SendDndAction(uint32_t action) override {
    NOTREACHED_IN_MIGRATION() << "The interface does not support this method.";
  }

  void SendDndDropPerformed() override {
    NOTREACHED_IN_MIGRATION() << "The interface does not support this method.";
  }

  raw_ptr<TestSelectionSource> source = nullptr;
};

struct GtkPrimarySelectionDeviceManager
    : public TestSelectionDeviceManager::Delegate {
  explicit GtkPrimarySelectionDeviceManager(uint32_t version)
      : version_(version) {}
  ~GtkPrimarySelectionDeviceManager() override = default;

  TestSelectionDevice* CreateDevice(wl_client* client, uint32_t id) override {
    static const struct gtk_primary_selection_device_interface
        kTestSelectionDeviceImpl = {&TestSelectionDevice::SetSelection,
                                    &Destroy};
    auto owned_delegate = std::make_unique<GtkPrimarySelectionDevice>();
    auto* delegate = owned_delegate.get();
    wl_resource* resource = CreateResourceWithImpl<TestSelectionDevice>(
        client, &gtk_primary_selection_device_interface, version_,
        &kTestSelectionDeviceImpl, id, std::move(owned_delegate));
    delegate->device = GetUserDataAs<TestSelectionDevice>(resource);
    return delegate->device;
  }

  TestSelectionSource* CreateSource(wl_client* client, uint32_t id) override {
    static const struct gtk_primary_selection_source_interface
        kTestSelectionSourceImpl = {&TestSelectionSource::Offer, &Destroy};
    auto owned_delegate = std::make_unique<GtkPrimarySelectionSource>();
    auto* delegate = owned_delegate.get();
    wl_resource* resource = CreateResourceWithImpl<TestSelectionSource>(
        client, &gtk_primary_selection_source_interface, version_,
        &kTestSelectionSourceImpl, id, std::move(owned_delegate));
    delegate->source = GetUserDataAs<TestSelectionSource>(resource);
    return delegate->source;
  }

 private:
  const uint32_t version_;
};

}  // namespace

std::unique_ptr<TestSelectionDeviceManager> CreateTestSelectionManagerGtk() {
  constexpr uint32_t kVersion = 1;
  static const struct gtk_primary_selection_device_manager_interface
      kTestSelectionManagerImpl = {&TestSelectionDeviceManager::CreateSource,
                                   &TestSelectionDeviceManager::GetDevice,
                                   &Destroy};
  static const TestSelectionDeviceManager::InterfaceInfo interface_info = {
      .interface = &gtk_primary_selection_device_manager_interface,
      .implementation = &kTestSelectionManagerImpl,
      .version = kVersion};
  return std::make_unique<TestSelectionDeviceManager>(
      interface_info,
      std::make_unique<GtkPrimarySelectionDeviceManager>(kVersion));
}

}  // namespace wl
