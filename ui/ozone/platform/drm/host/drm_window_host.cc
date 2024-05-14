// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/host/drm_window_host.h"

#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "ui/base/cursor/platform_cursor.h"
#include "ui/display/display.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/event.h"
#include "ui/events/ozone/evdev/event_factory_evdev.h"
#include "ui/events/ozone/events_ozone.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/ozone/common/bitmap_cursor.h"
#include "ui/ozone/platform/drm/host/drm_cursor.h"
#include "ui/ozone/platform/drm/host/drm_display_host.h"
#include "ui/ozone/platform/drm/host/drm_display_host_manager.h"
#include "ui/ozone/platform/drm/host/drm_window_host_manager.h"
#include "ui/ozone/platform/drm/host/gpu_thread_adapter.h"

namespace ui {

DrmWindowHost::DrmWindowHost(PlatformWindowDelegate* delegate,
                             const gfx::Rect& bounds,
                             GpuThreadAdapter* sender,
                             EventFactoryEvdev* event_factory,
                             DrmCursor* cursor,
                             DrmWindowHostManager* window_manager,
                             DrmDisplayHostManager* display_manager)
    : delegate_(delegate),
      sender_(sender),
      event_factory_(event_factory),
      cursor_(cursor),
      window_manager_(window_manager),
      display_manager_(display_manager),
      bounds_(bounds),
      widget_(window_manager->NextAcceleratedWidget()) {
  window_manager_->AddWindow(widget_, this);
}

DrmWindowHost::~DrmWindowHost() {
  PlatformEventSource::GetInstance()->RemovePlatformEventDispatcher(this);
  window_manager_->RemoveWindow(widget_);
  cursor_->OnWindowRemoved(widget_);

  sender_->RemoveGpuThreadObserver(this);
  sender_->GpuDestroyWindow(widget_);
}

void DrmWindowHost::Initialize() {
  sender_->AddGpuThreadObserver(this);
  PlatformEventSource::GetInstance()->AddPlatformEventDispatcher(this);
  cursor_->OnWindowAdded(widget_, bounds_, GetCursorConfinedBounds());
  delegate_->OnAcceleratedWidgetAvailable(widget_);
}

gfx::AcceleratedWidget DrmWindowHost::GetAcceleratedWidget() const {
  return widget_;
}

gfx::Rect DrmWindowHost::GetCursorConfinedBounds() const {
  return cursor_confined_bounds_.IsEmpty() ? gfx::Rect(bounds_.size())
                                           : cursor_confined_bounds_;
}

void DrmWindowHost::Show(bool inactive) {}

void DrmWindowHost::Hide() {}

void DrmWindowHost::Close() {}

bool DrmWindowHost::IsVisible() const {
  NOTREACHED_IN_MIGRATION();
  return true;
}

void DrmWindowHost::PrepareForShutdown() {}

void DrmWindowHost::SetBoundsInPixels(const gfx::Rect& bounds) {
  bool origin_changed = bounds_.origin() != bounds.origin();
  bounds_ = bounds;
  delegate_->OnBoundsChanged({origin_changed});
  SendBoundsChange();
}

gfx::Rect DrmWindowHost::GetBoundsInPixels() const {
  return bounds_;
}

void DrmWindowHost::SetBoundsInDIP(const gfx::Rect& bounds) {
  NOTREACHED_IN_MIGRATION();
  // No scaling at DRM level and should always use pixel bounds.
}

gfx::Rect DrmWindowHost::GetBoundsInDIP() const {
  // No scaling at DRM level and should always use pixel bounds.
  NOTREACHED_IN_MIGRATION();
  return bounds_;
}

void DrmWindowHost::SetTitle(const std::u16string& title) {}

void DrmWindowHost::SetCapture() {
  window_manager_->GrabEvents(widget_);
}

void DrmWindowHost::ReleaseCapture() {
  window_manager_->UngrabEvents(widget_);
}

bool DrmWindowHost::HasCapture() const {
  return widget_ == window_manager_->event_grabber();
}

void DrmWindowHost::SetFullscreen(bool fullscreen, int64_t target_display_id) {}

void DrmWindowHost::Maximize() {}

void DrmWindowHost::Minimize() {}

void DrmWindowHost::Restore() {}

PlatformWindowState DrmWindowHost::GetPlatformWindowState() const {
  return PlatformWindowState::kUnknown;
}

void DrmWindowHost::Activate() {
  NOTIMPLEMENTED_LOG_ONCE();
}

void DrmWindowHost::Deactivate() {
  NOTIMPLEMENTED_LOG_ONCE();
}

void DrmWindowHost::SetUseNativeFrame(bool use_native_frame) {}

bool DrmWindowHost::ShouldUseNativeFrame() const {
  return false;
}

void DrmWindowHost::SetCursor(scoped_refptr<PlatformCursor> cursor) {
  cursor_->SetCursor(widget_, BitmapCursor::FromPlatformCursor(cursor));
}

void DrmWindowHost::MoveCursorTo(const gfx::Point& location) {
  event_factory_->WarpCursorTo(widget_, gfx::PointF(location));
}

void DrmWindowHost::ConfineCursorToBounds(const gfx::Rect& bounds) {
  if (cursor_confined_bounds_ == bounds)
    return;

  cursor_confined_bounds_ = bounds;
  cursor_->CommitBoundsChange(widget_, bounds_, bounds);
}

void DrmWindowHost::SetRestoredBoundsInDIP(const gfx::Rect& bounds) {
  NOTREACHED_IN_MIGRATION();
}

gfx::Rect DrmWindowHost::GetRestoredBoundsInDIP() const {
  NOTREACHED_IN_MIGRATION();
  return gfx::Rect();
}

void DrmWindowHost::SetWindowIcons(const gfx::ImageSkia& window_icon,
                                   const gfx::ImageSkia& app_icon) {
  NOTREACHED_IN_MIGRATION();
}

void DrmWindowHost::SizeConstraintsChanged() {
  NOTREACHED_IN_MIGRATION();
}

void DrmWindowHost::OnMouseEnter() {
  delegate_->OnMouseEnter();
}

bool DrmWindowHost::CanDispatchEvent(const PlatformEvent& event) {
  DCHECK(event);

  // If there is a grab, capture events here.
  gfx::AcceleratedWidget grabber = window_manager_->event_grabber();
  if (grabber != gfx::kNullAcceleratedWidget)
    return grabber == widget_;

  if (event->IsTouchEvent()) {
    // Dispatch the event if it is from the touchscreen associated with the
    // DrmWindowHost. We cannot check the event's location because if the
    // touchscreen has a bezel, touches in the bezel have a location outside of
    // |bounds_|.
    int64_t display_id =
        DeviceDataManager::GetInstance()->GetTargetDisplayForTouchDevice(
            event->source_device_id());

    if (display_id == display::kInvalidDisplayId)
      return false;

    DrmDisplayHost* display = display_manager_->GetDisplay(display_id);
    if (!display)
      return false;

    display::DisplaySnapshot* snapshot = display->snapshot();
    if (!snapshot->current_mode())
      return false;

    gfx::Rect display_bounds(snapshot->origin(),
                             snapshot->current_mode()->size());
    return display_bounds == bounds_;
  } else if (event->IsLocatedEvent()) {
    LocatedEvent* located_event = event->AsLocatedEvent();
    return bounds_.Contains(located_event->location());
  }

  // TODO(spang): For non-ash builds we would need smarter keyboard focus.
  return true;
}

uint32_t DrmWindowHost::DispatchEvent(const PlatformEvent& event) {
  DCHECK(event);

  if (event->IsLocatedEvent()) {
    // Make the event location relative to this window's origin.
    LocatedEvent* located_event = event->AsLocatedEvent();

    if (event->IsMouseEvent()) {
      DrmWindowHost* window_on_mouse =
          window_manager_->GetWindowAt(located_event->location());
      if (window_on_mouse)
        window_manager_->MouseOnWindow(window_on_mouse);
    }

    gfx::PointF location = located_event->location_f();
    location -= gfx::Vector2dF(bounds_.OffsetFromOrigin());
    located_event->set_location_f(location);
    located_event->set_root_location_f(location);
  }
  DispatchEventFromNativeUiEvent(
      event, base::BindOnce(&PlatformWindowDelegate::DispatchEvent,
                            base::Unretained(delegate_)));
  return POST_DISPATCH_STOP_PROPAGATION;
}

void DrmWindowHost::OnGpuProcessLaunched() {}

void DrmWindowHost::OnGpuThreadReady() {
  sender_->GpuCreateWindow(widget_, bounds_);
}

void DrmWindowHost::OnGpuThreadRetired() {}

void DrmWindowHost::SendBoundsChange() {
  // Update the cursor before the window so that the cursor stays within the
  // window bounds when the window size shrinks.
  cursor_->CommitBoundsChange(widget_, bounds_, GetCursorConfinedBounds());
  sender_->GpuWindowBoundsChanged(widget_, bounds_);
}

}  // namespace ui
