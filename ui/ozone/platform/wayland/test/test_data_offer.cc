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
#include "ui/ozone/platform/wayland/test/constants.h"

namespace wl {

namespace {

void WriteDataOnWorkerThread(base::ScopedFD fd,
                             const ui::PlatformClipboard::Data& data) {
  if (!base::WriteFileDescriptor(
          fd.get(), reinterpret_cast<const char*>(data.data()), data.size())) {
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
  NOTIMPLEMENTED();
}

}  // namespace

const struct wl_data_offer_interface kTestDataOfferImpl = {
    DataOfferAccept, DataOfferReceive, DataOfferDestroy, DataOfferFinish,
    DataOfferSetActions};

TestDataOffer::TestDataOffer(wl_resource* resource)
    : ServerObject(resource),
      task_runner_(base::CreateSequencedTaskRunner(
          {base::ThreadPool(), base::MayBlock()})),
      write_data_weak_ptr_factory_(this) {}

TestDataOffer::~TestDataOffer() {}

void TestDataOffer::Receive(const std::string& mime_type, base::ScopedFD fd) {
  DCHECK(fd.is_valid());

  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(&WriteDataOnWorkerThread, std::move(fd),
                                        data_to_offer_[mime_type]));
}

void TestDataOffer::OnOffer(const std::string& mime_type,
                            const ui::PlatformClipboard::Data& data) {
  data_to_offer_[mime_type] = data;
  wl_data_offer_send_offer(resource(), mime_type.c_str());
}

}  // namespace wl
