// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_data_device.h"

#include <memory>
#include <string>
#include <utility>

#include "base/files/scoped_file.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "build/chromeos_buildflags.h"
#include "ui/events/base_event_utils.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"
#include "ui/ozone/platform/wayland/common/wayland_util.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_data_offer.h"
#include "ui/ozone/platform/wayland/host/wayland_data_source.h"
#include "ui/ozone/platform/wayland/host/wayland_exchange_data_provider.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"

namespace ui {

WaylandDataDevice::WaylandDataDevice(WaylandConnection* connection,
                                     wl_data_device* data_device)
    : WaylandDataDeviceBase(connection), data_device_(data_device) {
  static constexpr wl_data_device_listener kDataDeviceListener = {
      .data_offer = &OnDataOffer,
      .enter = &OnEnter,
      .leave = &OnLeave,
      .motion = &OnMotion,
      .drop = &OnDrop,
      .selection = &OnSelection};
  wl_data_device_add_listener(data_device_.get(), &kDataDeviceListener, this);
}

WaylandDataDevice::~WaylandDataDevice() = default;

void WaylandDataDevice::StartDrag(const WaylandDataSource& data_source,
                                  const WaylandWindow& origin_window,
                                  uint32_t serial,
                                  wl_surface* icon_surface,
                                  DragDelegate* delegate) {
  DCHECK(delegate);
  CHECK(!drag_delegate_);
  drag_delegate_ = delegate;

  wl_data_device_start_drag(data_device_.get(), data_source.data_source(),
                            origin_window.root_surface()->surface(),
                            icon_surface, serial);
  drag_delegate_->DrawIcon();
  connection()->Flush();
}

void WaylandDataDevice::ResetDragDelegate() {
  drag_delegate_ = nullptr;
}

void WaylandDataDevice::ResetDragDelegateIfNotDragSource() {
  // When in an active drag-and-drop session initiated by an external Wayland
  // client, |drag_delegate_| is set at OnEnter, and must be reset upon
  // OnLeave/OnDrop in order to avoid potential memory corruption issues.
  if (drag_delegate_ && !drag_delegate_->IsDragSource()) {
    ResetDragDelegate();
  }
}

void WaylandDataDevice::SetSelectionSource(WaylandDataSource* source,
                                           uint32_t serial) {
  auto* data_source = source ? source->data_source() : nullptr;
  wl_data_device_set_selection(data_device_.get(), data_source, serial);
  connection()->Flush();
}

bool WaylandDataDevice::IsDragInProgress() const {
  // Starting wayland drag sessions require clients to provide a drag delegate.
  // The delegate is explicitly reset by the client when the wayland drag
  // session has ended.
  return !!drag_delegate_;
}

// static
void WaylandDataDevice::OnDataOffer(void* data,
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
  // drag enter event doesn't have timestamp. Use EventTimeForNow().
  const auto timestamp = EventTimeForNow();

  // Null |drag_delegate_| here means that the DND session has been initiated by
  // an external application. In this case, use the default data drag delegate.
  if (!self->drag_delegate_)
    self->drag_delegate_ = self->connection()->data_drag_controller();

  DCHECK(self->new_offer_);
  self->drag_delegate_->OnDragOffer(std::move(self->new_offer_));

  gfx::PointF point = self->connection()->MaybeConvertLocation(
      gfx::PointF(wl_fixed_to_double(x), wl_fixed_to_double(y)), window);
  self->drag_delegate_->OnDragEnter(window, point, timestamp, serial);

  self->connection()->Flush();
}

void WaylandDataDevice::OnMotion(void* data,
                                 wl_data_device* data_device,
                                 uint32_t time,
                                 wl_fixed_t x,
                                 wl_fixed_t y) {
  auto* self = static_cast<WaylandDataDevice*>(data);
  if (self->drag_delegate_) {
    gfx::PointF point = self->connection()->MaybeConvertLocation(
        gfx::PointF(wl_fixed_to_double(x), wl_fixed_to_double(y)),
        self->drag_delegate_->GetDragTarget());
    self->drag_delegate_->OnDragMotion(point,
                                       wl::EventMillisecondsToTimeTicks(time));
  }
}

void WaylandDataDevice::OnDrop(void* data, wl_data_device* data_device) {
  // drop event doesn't have timestamp. Use EventTimeForNow().
  const auto timestamp = EventTimeForNow();
  auto* self = static_cast<WaylandDataDevice*>(data);
  if (self->drag_delegate_) {
    self->drag_delegate_->OnDragDrop(timestamp);
    self->connection()->Flush();
  }

  // There are buggy Exo versions, which send 'drop' event (even for
  // unsuccessful drops) without a subsequent 'leave'. In order to mitigate
  // potential leaks and/or UAFs, forcibly call corresponding delegate callback
  // here, in Lacros. TODO(crbug.com/40819972): Remove once Exo bug is fixed.
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // Delegate might have been reset already if the drag was cancelled in
  // response to the drop event.
  if (self->drag_delegate_) {
    self->drag_delegate_->OnDragLeave(timestamp);
    self->ResetDragDelegateIfNotDragSource();
  }
#endif
}

void WaylandDataDevice::OnLeave(void* data, wl_data_device* data_device) {
  // leave event doesn't have timestamp. Use EventTimeForNow().
  const auto timestamp = EventTimeForNow();
  auto* self = static_cast<WaylandDataDevice*>(data);
  if (self->drag_delegate_) {
    self->drag_delegate_->OnDragLeave(timestamp);
    self->connection()->Flush();
  }
  self->ResetDragDelegateIfNotDragSource();
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

  self->NotifySelectionOffer(self->data_offer());
}

}  // namespace ui
