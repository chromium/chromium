// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_data_device_base.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/task/task_traits.h"
#include "ui/ozone/platform/wayland/common/wayland_util.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_data_offer_base.h"
#include "ui/ozone/platform/wayland/host/wayland_serial_tracker.h"

namespace ui {

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

void WaylandDataDeviceBase::ResetDataOffer() {
  data_offer_.reset();
}

PlatformClipboard::Data WaylandDataDeviceBase::ReadFromFD(
    base::ScopedFD fd) const {
  std::vector<uint8_t> contents;
  wl::ReadDataFromFD(std::move(fd), &contents);
  return base::RefCountedBytes::TakeVector(&contents);
}

void WaylandDataDeviceBase::RegisterDeferredReadCallback() {
  DCHECK(!deferred_read_callback_);

  deferred_read_callback_.reset(
      wl_display_sync(connection_->display_wrapper()));

  static constexpr wl_callback_listener kListener = {&DeferredReadCallback};

  wl_callback_add_listener(deferred_read_callback_.get(), &kListener, this);

  connection_->Flush();
}

void WaylandDataDeviceBase::RegisterDeferredReadClosure(
    base::OnceClosure closure) {
  deferred_read_closure_ = std::move(closure);
}

// static
void WaylandDataDeviceBase::DeferredReadCallback(void* data,
                                                 struct wl_callback* cb,
                                                 uint32_t time) {
  auto* data_device = static_cast<WaylandDataDeviceBase*>(data);
  DCHECK(data_device);
  data_device->DeferredReadCallbackInternal(cb, time);
}

void WaylandDataDeviceBase::DeferredReadCallbackInternal(struct wl_callback* cb,
                                                         uint32_t time) {
  DCHECK(!deferred_read_closure_.is_null());

  // The callback must be reset before invoking the closure because the latter
  // may want to set another callback.  That typically happens when
  // non-trivial data types are dropped; they have fallbacks to plain text so
  // several roundtrips to data are chained.
  deferred_read_callback_.reset();

  std::move(deferred_read_closure_).Run();
}

void WaylandDataDeviceBase::NotifySelectionOffer(
    WaylandDataOfferBase* offer) const {
  if (selection_offer_callback_)
    selection_offer_callback_.Run(offer);
}

}  // namespace ui
