// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/test_data_source.h"

#include <wayland-server-core.h>

#include <memory>
#include <string>

#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "ui/ozone/platform/wayland/test/test_selection_device_manager.h"
#include "ui/ozone/platform/wayland/test/test_wayland_server_thread.h"

namespace wl {

namespace {

void DataSourceDestroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void DataSourceSetActions(wl_client* client,
                          wl_resource* resource,
                          uint32_t dnd_actions) {
  GetUserDataAs<TestDataSource>(resource)->SetActions(dnd_actions);
}

struct WlDataSourceImpl : public TestSelectionSource::Delegate {
  explicit WlDataSourceImpl(TestDataSource* source) : source_(source) {}
  ~WlDataSourceImpl() override = default;

  WlDataSourceImpl(const WlDataSourceImpl&) = delete;
  WlDataSourceImpl& operator=(const WlDataSourceImpl&) = delete;

  void SendSend(const std::string& mime_type,
                base::ScopedFD write_fd) override {
    wl_data_source_send_send(source_->resource(), mime_type.c_str(),
                             write_fd.get());
    wl_client_flush(wl_resource_get_client(source_->resource()));
  }

  void SendFinished() override {
    wl_data_source_send_dnd_finished(source_->resource());
    wl_client_flush(wl_resource_get_client(source_->resource()));
  }

  void SendCancelled() override {
    wl_data_source_send_cancelled(source_->resource());
    wl_client_flush(wl_resource_get_client(source_->resource()));
  }

  void SendDndAction(uint32_t action) override {
    wl_data_source_send_action(source_->resource(), action);
    wl_client_flush(wl_resource_get_client(source_->resource()));
  }

  void SendDndDropPerformed() override {
    wl_data_source_send_dnd_drop_performed(source_->resource());
    wl_client_flush(wl_resource_get_client(source_->resource()));
  }

 private:
  const raw_ptr<TestDataSource> source_;
};

}  // namespace

const struct wl_data_source_interface kTestDataSourceImpl = {
    TestSelectionSource::Offer, DataSourceDestroy, DataSourceSetActions};

TestDataSource::TestDataSource(wl_resource* resource)
    : TestSelectionSource(resource, std::make_unique<WlDataSourceImpl>(this)) {}

TestDataSource::~TestDataSource() = default;

void TestDataSource::SetActions(uint32_t dnd_actions) {
  actions_ |= dnd_actions;
}

}  // namespace wl
