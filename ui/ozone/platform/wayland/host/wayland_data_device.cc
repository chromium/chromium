// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_data_device.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/files/scoped_file.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/ozone/platform/wayland/common/data_util.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"
#include "ui/ozone/platform/wayland/common/wayland_util.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_data_offer.h"
#include "ui/ozone/platform/wayland/host/wayland_data_source.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"

namespace ui {

WaylandDataDevice::WaylandDataDevice(WaylandConnection* connection,
                                     wl_data_device* data_device)
    : WaylandDataDeviceBase(connection), data_device_(data_device) {
  static const struct wl_data_device_listener kDataDeviceListener = {
      WaylandDataDevice::OnOffer, WaylandDataDevice::OnEnter,
      WaylandDataDevice::OnLeave, WaylandDataDevice::OnMotion,
      WaylandDataDevice::OnDrop,  WaylandDataDevice::OnSelection};
  wl_data_device_add_listener(data_device_.get(), &kDataDeviceListener, this);
}

WaylandDataDevice::~WaylandDataDevice() = default;

void WaylandDataDevice::StartDrag(const WaylandDataSource& data_source,
                                  const WaylandWindow& origin_window,
                                  wl_surface* icon_surface,
                                  DragDelegate* delegate) {
  DCHECK(delegate);
  DCHECK(!drag_delegate_);
  drag_delegate_ = delegate;

  wl_data_device_start_drag(data_device_.get(), data_source.data_source(),
                            origin_window.root_surface()->surface(),
                            icon_surface, connection()->serial());
  drag_delegate_->DrawIcon();
  connection()->ScheduleFlush();
}

void WaylandDataDevice::ResetDragDelegate() {
  DCHECK(drag_delegate_);
  drag_delegate_ = nullptr;
}

void WaylandDataDevice::RequestData(WaylandDataOffer* offer,
                                    const std::string& mime_type,
                                    RequestDataCallback callback) {
  DCHECK(offer);
  DCHECK(wl::IsMimeTypeSupported(mime_type));

  base::ScopedFD fd = offer->Receive(mime_type);
  if (!fd.is_valid()) {
    LOG(ERROR) << "Failed to open file descriptor.";
    return;
  }

  // Ensure there is not pending operation to be performed by the compositor,
  // otherwise read(..) can block awaiting data to be sent to pipe.
  RegisterDeferredReadClosure(base::BindOnce(
      &WaylandDataDevice::ReadDragDataFromFD, base::Unretained(this),
      std::move(fd), std::move(callback)));
  RegisterDeferredReadCallback();
}

void WaylandDataDevice::SetSelectionSource(WaylandDataSource* source) {
  DCHECK(source);
  wl_data_device_set_selection(data_device_.get(), source->data_source(),
                               connection()->serial());
  connection()->ScheduleFlush();
}

void WaylandDataDevice::ReadDragDataFromFD(base::ScopedFD fd,
                                           RequestDataCallback callback) {
  std::vector<uint8_t> contents;
  wl::ReadDataFromFD(std::move(fd), &contents);
  std::move(callback).Run(scoped_refptr<base::RefCountedBytes>(
      base::RefCountedBytes::TakeVector(&contents)));
}

// static
void WaylandDataDevice::OnOffer(void* data,
                                wl_data_device* data_device,
                                wl_data_offer* offer) {
  auto* self = static_cast<WaylandDataDevice*>(data);
  DCHECK(self);
  DCHECK(!self->new_offer_);
  self->new_offer_ = std::make_unique<WaylandDataOffer>(offer);
}

void WaylandDataDevice::OnEnter(void* data,
                                wl_data_device* data_device,
                                uint32_t serial,
                                wl_surface* surface,
                                wl_fixed_t x,
                                wl_fixed_t y,
                                wl_data_offer* offer) {
  auto* self = static_cast<WaylandDataDevice*>(data);
  WaylandWindow* window = wl::RootWindowFromWlSurface(surface);

  // During Chrome's tab dragging, when a browser window is quickly snapped in
  // and out, it might get destroyed before the wl_data_device::enter event is
  // processed for a drag offer. If this happens, |window| will be null here, so
  // destroy |new_offer_| here, as some compositors assume it. Such behavior has
  // been observed in Exosphere compositor, for example.
  if (!window) {
    self->new_offer_.reset();
    VLOG(1) << "Failed to get window.";
    return;
  }

  // Null |drag_delegate_| here means that the DND session has been initiated by
  // an external application. In this case, use the default data drag delegate.
  if (!self->drag_delegate_)
    self->drag_delegate_ = self->connection()->data_drag_controller();

  DCHECK(self->new_offer_);
  self->drag_delegate_->OnDragOffer(std::move(self->new_offer_));

  gfx::PointF point(wl_fixed_to_double(x), wl_fixed_to_double(y));
  self->drag_delegate_->OnDragEnter(window, point, serial);
}

void WaylandDataDevice::OnMotion(void* data,
                                 wl_data_device* data_device,
                                 uint32_t time,
                                 wl_fixed_t x,
                                 wl_fixed_t y) {
  auto* self = static_cast<WaylandDataDevice*>(data);
  if (self->drag_delegate_) {
    gfx::PointF point(wl_fixed_to_double(x), wl_fixed_to_double(y));
    self->drag_delegate_->OnDragMotion(point);
  }
}

void WaylandDataDevice::OnDrop(void* data, wl_data_device* data_device) {
  auto* self = static_cast<WaylandDataDevice*>(data);
  if (self->drag_delegate_)
    self->drag_delegate_->OnDragDrop();
}

void WaylandDataDevice::OnLeave(void* data, wl_data_device* data_device) {
  auto* self = static_cast<WaylandDataDevice*>(data);
  if (self->drag_delegate_)
    self->drag_delegate_->OnDragLeave();

  // When in a DND session initiated by an external application,
  // |drag_delegate_| is set at OnEnter, and must be reset here to avoid
  // potential use-after-free. Above call to OnDragLeave() may result in
  // |drag_delegate_| being reset, so it must be checked here as well.
  if (self->drag_delegate_ && !self->drag_delegate_->IsDragSource())
    self->ResetDragDelegate();
}

void WaylandDataDevice::OnSelection(void* data,
                                    wl_data_device* data_device,
                                    wl_data_offer* offer) {
  auto* self = static_cast<WaylandDataDevice*>(data);
  DCHECK(self);

  // 'offer' will be null to indicate that the selection is no longer valid,
  // i.e. there is no longer selection data available to fetch.
  if (!offer) {
    self->ResetDataOffer();
  } else {
    DCHECK(self->new_offer_);
    self->set_data_offer(std::move(self->new_offer_));
    self->data_offer()->EnsureTextMimeTypeIfNeeded();
  }

  if (self->selection_delegate())
    self->selection_delegate()->OnSelectionOffer(self->data_offer());
}

}  // namespace ui
