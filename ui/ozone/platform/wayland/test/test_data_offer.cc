// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/test_data_offer.h"

#include <wayland-server-core.h>
#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "ui/ozone/platform/wayland/test/test_selection_device_manager.h"

namespace wl {

namespace {

void DataOfferAccept(wl_client* client,
                     wl_resource* resource,
                     uint32_t serial,
                     const char* mime_type) {
  NOTIMPLEMENTED();
}

void DataOfferDestroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void DataOfferFinish(wl_client* client, wl_resource* resource) {
  NOTIMPLEMENTED();
}

void DataOfferSetActions(wl_client* client,
                         wl_resource* resource,
                         uint32_t dnd_actions,
                         uint32_t preferred_action) {
  GetUserDataAs<TestDataOffer>(resource)->SetActions(dnd_actions,
                                                     preferred_action);
}

struct WlDataOfferImpl : public TestSelectionOffer::Delegate {
  explicit WlDataOfferImpl(TestDataOffer* offer) : offer_(offer) {}
  ~WlDataOfferImpl() override = default;

  WlDataOfferImpl(const WlDataOfferImpl&) = delete;
  WlDataOfferImpl& operator=(const WlDataOfferImpl&) = delete;

  void SendOffer(const std::string& mime_type) override {
    wl_data_offer_send_offer(offer_->resource(), mime_type.c_str());
  }

 private:
  const raw_ptr<TestDataOffer> offer_;
};

}  // namespace

const struct wl_data_offer_interface kTestDataOfferImpl = {
    DataOfferAccept, &TestSelectionOffer::Receive, DataOfferDestroy,
    DataOfferFinish, DataOfferSetActions};

TestDataOffer::TestDataOffer(wl_resource* resource)
    : TestSelectionOffer(resource, std::make_unique<WlDataOfferImpl>(this)) {}

TestDataOffer::~TestDataOffer() = default;

// static
TestDataOffer* TestDataOffer::FromResource(wl_resource* resource) {
  if (!ResourceHasImplementation(resource, &wl_data_offer_interface,
                                 &kTestDataOfferImpl)) {
    return nullptr;
  }
  return GetUserDataAs<TestDataOffer>(resource);
}

void TestDataOffer::SetActions(uint32_t dnd_actions,
                               uint32_t preferred_action) {
  client_supported_actions_ = dnd_actions;
  client_preferred_action_ = preferred_action;
  OnAction(client_preferred_action_);
}

void TestDataOffer::OnSourceActions(uint32_t source_actions) {
  wl_data_offer_send_source_actions(resource(), source_actions);
}

void TestDataOffer::OnAction(uint32_t dnd_action) {
  wl_data_offer_send_action(resource(), dnd_action);
}

}  // namespace wl
