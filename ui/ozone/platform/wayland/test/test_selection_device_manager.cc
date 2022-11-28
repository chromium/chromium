// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/test_selection_device_manager.h"

#include <wayland-server-core.h>

#include <cstdint>
#include <vector>

#include "base/bind.h"
#include "base/check.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "ui/ozone/platform/wayland/test/server_object.h"

namespace wl {

namespace {

std::vector<uint8_t> ReadDataOnWorkerThread(base::ScopedFD fd) {
  constexpr size_t kChunkSize = 1024;
  std::vector<uint8_t> bytes;
  while (true) {
    uint8_t chunk[kChunkSize];
    ssize_t bytes_read = HANDLE_EINTR(read(fd.get(), chunk, kChunkSize));
    if (bytes_read > 0) {
      bytes.insert(bytes.end(), chunk, chunk + bytes_read);
      continue;
    }
    if (bytes_read < 0) {
      PLOG(ERROR) << "Failed to read data";
      bytes.clear();
    }
    break;
  }
  return bytes;
}

void WriteDataOnWorkerThread(base::ScopedFD fd,
                             ui::PlatformClipboard::Data data) {
  if (!base::WriteFileDescriptor(fd.get(), data->data())) {
    LOG(ERROR) << "Failed to write selection data to clipboard.";
  }
}

}  //  namespace

// TestSelectionOffer implementation.
TestSelectionOffer::TestSelectionOffer(wl_resource* resource,
                                       Delegate* delegate)
    : ServerObject(resource),
      delegate_(delegate),
      task_runner_(
          base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()})) {}

TestSelectionOffer::~TestSelectionOffer() {
  delegate_->OnDestroying();
}

void TestSelectionOffer::OnOffer(const std::string& mime_type,
                                 ui::PlatformClipboard::Data data) {
  data_to_offer_[mime_type] = data;
  delegate_->SendOffer(mime_type);
}

void TestSelectionOffer::Receive(wl_client* client,
                                 wl_resource* resource,
                                 const char* mime_type,
                                 int fd) {
  CHECK(GetUserDataAs<TestSelectionOffer>(resource));
  auto* self = GetUserDataAs<TestSelectionOffer>(resource);
  self->task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&WriteDataOnWorkerThread, base::ScopedFD(fd),
                                self->data_to_offer_[mime_type]));
}

// TestSelectionSource implementation.
TestSelectionSource::TestSelectionSource(wl_resource* resource,
                                         Delegate* delegate)
    : ServerObject(resource),
      delegate_(delegate),
      task_runner_(
          base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()})) {}

TestSelectionSource::~TestSelectionSource() {
  delegate_->OnDestroying();
}

void TestSelectionSource::ReadData(const std::string& mime_type,
                                   ReadDataCallback callback) {
  base::ScopedFD read_fd;
  base::ScopedFD write_fd;
  PCHECK(base::CreatePipe(&read_fd, &write_fd));

  // 1. Send the SEND event to notify client's DataSource that it's time
  // to send us the drag data thrhough the write_fd file descriptor.
  delegate_->SendSend(mime_type, std::move(write_fd));

  // 2. Schedule the ReadDataOnWorkerThread task. The result of read
  // operation will be then passed in to the callback requested by the caller.
  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&ReadDataOnWorkerThread, std::move(read_fd)),
      std::move(callback));
}

void TestSelectionSource::OnFinished() {
  delegate_->SendFinished();
  mime_types_.clear();
}

void TestSelectionSource::OnCancelled() {
  delegate_->SendCancelled();
  mime_types_.clear();
}

void TestSelectionSource::OnDndAction(uint32_t action) {
  delegate_->SendDndAction(action);
}

void TestSelectionSource::Offer(struct wl_client* client,
                                struct wl_resource* resource,
                                const char* mime_type) {
  CHECK(GetUserDataAs<TestSelectionSource>(resource));
  auto* self = GetUserDataAs<TestSelectionSource>(resource);
  self->mime_types_.push_back(mime_type);
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
  self->selection_serial_ = serial;
  self->delegate_->HandleSetSelection(src, serial);
  if (self->manager_)
    self->manager_->set_source(src);
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
  manager->delegate_->CreateSource(client, id);
}

void TestSelectionDeviceManager::GetDevice(wl_client* client,
                                           wl_resource* manager_resource,
                                           uint32_t id,
                                           wl_resource* seat_resource) {
  CHECK(GetUserDataAs<TestSelectionDeviceManager>(manager_resource));
  auto* manager = GetUserDataAs<TestSelectionDeviceManager>(manager_resource);
  auto* new_device = manager->delegate_->CreateDevice(client, id);
  new_device->set_manager(manager);
  manager->device_ = new_device;
}

}  // namespace wl
