// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_data_device_base.h"

#include <utility>

#include "base/logging.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "ui/ozone/platform/wayland/common/wayland_util.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_data_offer_base.h"

namespace ui {

namespace {

PlatformClipboard::Data ReadFromFD(base::ScopedFD fd) {
  std::vector<uint8_t> contents;
  wl::ReadDataFromFD(std::move(fd), &contents);
  return base::MakeRefCounted<base::RefCountedBytes>(std::move(contents));
}

}  // namespace

WaylandDataDeviceBase::WaylandDataDeviceBase(WaylandConnection* connection)
    : connection_(connection) {}

WaylandDataDeviceBase::~WaylandDataDeviceBase() = default;

const std::vector<std::string>& WaylandDataDeviceBase::GetAvailableMimeTypes()
    const {
  if (!data_offer_) {
    static std::vector<std::string> dummy;
    return dummy;
  }
  return data_offer_->mime_types();
}

PlatformClipboard::Data WaylandDataDeviceBase::ReadSelectionData(
    const std::string& mime_type) {
  if (!data_offer_)
    return {};

  base::ScopedFD fd = data_offer_->Receive(mime_type);
  connection_->Flush();

  if (!fd.is_valid()) {
    DPLOG(ERROR) << "Failed to open file descriptor.";
    return {};
  }

  // Do a roundtrip to ensure the above request reaches the server and the
  // resulting events get processed. Otherwise, the source client won’t send any
  // data, thus getting the owning thread stuck at the blocking read call below.
  connection_->RoundTripQueue();

  return ReadFromFD(std::move(fd));
}

void WaylandDataDeviceBase::RequestSelectionData(
    const std::string& mime_type,
    PlatformClipboard::RequestDataClosure callback) {
  if (!data_offer_) {
    std::move(callback).Run({});
    return;
  }

  base::ScopedFD fd = data_offer_->Receive(mime_type);
  connection_->Flush();

  if (!fd.is_valid()) {
    DPLOG(ERROR) << "Failed to open file descriptor.";
    std::move(callback).Run({});
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&ReadFromFD, std::move(fd)), std::move(callback));
}

void WaylandDataDeviceBase::ResetDataOffer() {
  data_offer_.reset();
}

void WaylandDataDeviceBase::NotifySelectionOffer(
    WaylandDataOfferBase* offer) const {
  if (selection_offer_callback_)
    selection_offer_callback_.Run(offer);
}

}  // namespace ui
