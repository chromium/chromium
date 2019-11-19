// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/internal/wayland_data_device_base.h"

#include <utility>

#include "base/bind.h"
#include "ui/ozone/platform/wayland/common/wayland_util.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"

namespace ui {
namespace internal {

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

bool WaylandDataDeviceBase::RequestSelectionData(const std::string& mime_type) {
  if (!data_offer_)
    return false;

  base::ScopedFD fd = data_offer_->Receive(mime_type);
  if (!fd.is_valid()) {
    LOG(ERROR) << "Failed to open file descriptor.";
    return false;
  }

  // Ensure there is not pending operation to be performed by the compositor,
  // otherwise read(..) can block awaiting data to be sent to pipe.
  RegisterDeferredReadClosure(
      base::BindOnce(&GtkPrimarySelectionDevice::ReadClipboardDataFromFD,
                     base::Unretained(this), std::move(fd), mime_type));
  RegisterDeferredReadCallback();
  return true;
}

void WaylandDataDeviceBase::ResetDataOffer() {
  data_offer_.reset();
}

void WaylandDataDeviceBase::ReadClipboardDataFromFD(
    base::ScopedFD fd,
    const std::string& mime_type) {
  std::vector<uint8_t> contents;
  wl::ReadDataFromFD(std::move(fd), &contents);
  connection_->clipboard()->SetData(contents, mime_type);
}

void WaylandDataDeviceBase::RegisterDeferredReadCallback() {
  DCHECK(!deferred_read_callback_);

  deferred_read_callback_.reset(wl_display_sync(connection_->display()));

  static const wl_callback_listener kListener = {
      GtkPrimarySelectionDevice::DeferredReadCallback};

  wl_callback_add_listener(deferred_read_callback_.get(), &kListener, this);

  connection_->ScheduleFlush();
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
  // may want to set another callback.  That typically happens when non-trivial
  // data types are dropped; they have fallbacks to plain text so several
  // roundtrips to data are chained.
  deferred_read_callback_.reset();

  std::move(deferred_read_closure_).Run();
}

}  // namespace internal
}  // namespace ui
