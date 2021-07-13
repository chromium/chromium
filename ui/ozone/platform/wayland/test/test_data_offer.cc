// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/test_data_offer.h"

#include <wayland-server-core.h>
#include <utility>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/sequenced_task_runner.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "ui/ozone/platform/wayland/test/test_selection_device_manager.h"

namespace wl {

namespace {

void WriteDataOnWorkerThread(base::ScopedFD fd,
                             ui::PlatformClipboard::Data data) {
  if (!base::WriteFileDescriptor(fd.get(), data->data())) {
    LOG(ERROR) << "Failed to write selection data to clipboard.";
  }
}

void DataOfferAccept(wl_client* client,
                     wl_resource* resource,
                     uint32_t serial,
                     const char* mime_type) {
  NOTIMPLEMENTED();
}

void DataOfferReceive(wl_client* client,
                      wl_resource* resource,
                      const char* mime_type,
                      int fd) {
  GetUserDataAs<TestDataOffer>(resource)->Receive(mime_type,
                                                  base::ScopedFD(fd));
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

  void SendOffer(const std::string& mime_type,
                 ui::PlatformClipboard::Data data) override {
    offer_->AddData(mime_type, data);
    wl_data_offer_send_offer(offer_->resource(), mime_type.c_str());
  }

  void OnDestroying() override { delete this; }

 private:
  TestDataOffer* const offer_;
};

}  // namespace

const struct wl_data_offer_interface kTestDataOfferImpl = {
    DataOfferAccept, DataOfferReceive, DataOfferDestroy, DataOfferFinish,
    DataOfferSetActions};

TestDataOffer::TestDataOffer(wl_resource* resource)
    : TestSelectionOffer(resource, new WlDataOfferImpl(this)),
      task_runner_(
          base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()})),
      write_data_weak_ptr_factory_(this) {}

TestDataOffer::~TestDataOffer() = default;

void TestDataOffer::Receive(const std::string& mime_type, base::ScopedFD fd) {
  DCHECK(fd.is_valid());

  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(&WriteDataOnWorkerThread, std::move(fd),
                                        data_to_offer_[mime_type]));
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

void TestDataOffer::AddData(const std::string& mime_type,
                            ui::PlatformClipboard::Data data) {
  data_to_offer_[mime_type] = data;
}

}  // namespace wl
