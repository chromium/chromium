// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/test_selection_device_manager.h"

#include <wayland-server-core.h>

#include "base/check.h"
#include "base/notreached.h"
#include "ui/ozone/platform/wayland/test/server_object.h"

namespace wl {

// TestSelectionOffer implementation.
TestSelectionOffer::TestSelectionOffer(wl_resource* resource,
                                       Delegate* delegate)
    : ServerObject(resource), delegate_(delegate) {}

TestSelectionOffer::~TestSelectionOffer() {
  delegate_->OnDestroying();
}

void TestSelectionOffer::OnOffer(const std::string& mime_type,
                                 ui::PlatformClipboard::Data data) {
  delegate_->SendOffer(mime_type, data);
}

// TestSelectionSource implementation.
TestSelectionSource::TestSelectionSource(wl_resource* resource,
                                         Delegate* delegate)
    : ServerObject(resource), delegate_(delegate) {}

TestSelectionSource::~TestSelectionSource() = default;

void TestSelectionSource::Offer(struct wl_client* client,
                                struct wl_resource* resource,
                                const char* mime_type) {
  CHECK(GetUserDataAs<TestSelectionSource>(resource));
  auto* self = GetUserDataAs<TestSelectionSource>(resource);
  self->delegate_->HandleOffer(mime_type);
}

// TestSelectionDevice implementation.
TestSelectionDevice::TestSelectionDevice(wl_resource* resource,
                                         Delegate* delegate)
    : ServerObject(resource), delegate_(delegate) {}

TestSelectionDevice::~TestSelectionDevice() {
  delegate_->OnDestroying();
}

TestSelectionOffer* TestSelectionDevice::OnDataOffer() {
  return delegate_->CreateAndSendOffer();
}

void TestSelectionDevice::OnSelection(TestSelectionOffer* offer) {
  delegate_->SendSelection(offer);
}

void TestSelectionDevice::SetSelection(struct wl_client* client,
                                       struct wl_resource* resource,
                                       struct wl_resource* source,
                                       uint32_t serial) {
  CHECK(GetUserDataAs<TestSelectionDevice>(resource));
  auto* self = GetUserDataAs<TestSelectionDevice>(resource);
  auto* src = source ? GetUserDataAs<TestSelectionSource>(source) : nullptr;
  self->delegate_->HandleSetSelection(src, serial);
}

TestSelectionDeviceManager::TestSelectionDeviceManager(
    const InterfaceInfo& info,
    Delegate* delegate)
    : GlobalObject(info.interface, info.implementation, info.version),
      delegate_(delegate) {}

TestSelectionDeviceManager::~TestSelectionDeviceManager() {
  delegate_->OnDestroying();
}

void TestSelectionDeviceManager::CreateSource(wl_client* client,
                                              wl_resource* manager_resource,
                                              uint32_t id) {
  CHECK(GetUserDataAs<TestSelectionDeviceManager>(manager_resource));
  auto* manager = GetUserDataAs<TestSelectionDeviceManager>(manager_resource);
  manager->source_ = manager->delegate_->CreateSource(client, id);
}

void TestSelectionDeviceManager::GetDevice(wl_client* client,
                                           wl_resource* manager_resource,
                                           uint32_t id,
                                           wl_resource* seat_resource) {
  CHECK(GetUserDataAs<TestSelectionDeviceManager>(manager_resource));
  auto* manager = GetUserDataAs<TestSelectionDeviceManager>(manager_resource);
  manager->device_ = manager->delegate_->CreateDevice(client, id);
}

}  // namespace wl
