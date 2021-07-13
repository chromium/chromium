// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/test_data_source.h"

#include <wayland-server-core.h>

#include <cstdint>
#include <utility>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/sequenced_task_runner.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/task_runner_util.h"
#include "ui/ozone/platform/wayland/test/test_selection_device_manager.h"

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

void DataSourceDestroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void DataSourceSetActions(wl_client* client,
                          wl_resource* resource,
                          uint32_t dnd_actions) {
  GetUserDataAs<TestDataSource>(resource)->SetActions(dnd_actions);
}

struct WlDataSourceImpl : public TestSelectionSource::Delegate {
  explicit WlDataSourceImpl(TestDataSource* offer) : source_(offer) {}
  ~WlDataSourceImpl() override = default;

  WlDataSourceImpl(const WlDataSourceImpl&) = delete;
  WlDataSourceImpl& operator=(const WlDataSourceImpl&) = delete;

  void HandleOffer(const std::string& mime_type) override {
    source_->Offer(mime_type);
  }

  void OnDestroying() override { delete this; }

 private:
  TestDataSource* const source_;
};

}  // namespace

const struct wl_data_source_interface kTestDataSourceImpl = {
    TestSelectionSource::Offer, DataSourceDestroy, DataSourceSetActions};

TestDataSource::TestDataSource(wl_resource* resource)
    : TestSelectionSource(resource, new WlDataSourceImpl(this)),
      task_runner_(
          base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()})) {}

TestDataSource::~TestDataSource() {}

void TestDataSource::Offer(const std::string& mime_type) {
  mime_types_.push_back(mime_type);
}

void TestDataSource::SetActions(uint32_t dnd_actions) {
  actions_ |= dnd_actions;
}

void TestDataSource::ReadData(const std::string& mime_type,
                              ReadDataCallback callback) {
  base::ScopedFD read_fd;
  base::ScopedFD write_fd;
  PCHECK(base::CreatePipe(&read_fd, &write_fd));

  // 1. Send the SEND event to notify client's DataSource that it's time
  // to send us the drag data thrhough the write_fd file descriptor.
  wl_data_source_send_send(resource(), mime_type.c_str(), write_fd.get());
  wl_client_flush(wl_resource_get_client(resource()));

  // 2. Schedule the ReadDataOnWorkerThread task. The result of read
  // operation will be then passed in to the callback requested by the caller.
  base::PostTaskAndReplyWithResult(
      task_runner_.get(), FROM_HERE,
      base::BindOnce(&ReadDataOnWorkerThread, std::move(read_fd)),
      std::move(callback));
}

void TestDataSource::OnCancelled() {
  wl_data_source_send_cancelled(resource());
}

}  // namespace wl
