// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/gesture_detection/filtered_gesture_provider.h"

#include "base/auto_reset.h"
#include "base/logging.h"
#include "ui/events/gesture_detection/motion_event.h"

namespace ui {

FilteredGestureProvider::TouchHandlingResult::TouchHandlingResult()
    : succeeded(false), moved_beyond_slop_region(false) {
}

FilteredGestureProvider::FilteredGestureProvider(
    const GestureProvider::Config& config,
    GestureProviderClient* client)
    : client_(client),
      gesture_provider_(std::make_unique<GestureProvider>(config, this)),
      gesture_filter_(this),
      handling_event_(false),
      any_touch_moved_beyond_slop_region_(false) {}

FilteredGestureProvider::~FilteredGestureProvider() = default;

void FilteredGestureProvider::UpdateConfig(
    const GestureProvider::Config& config) {
  gesture_provider_ = std::make_unique<ui::GestureProvider>(config, this);
}

FilteredGestureProvider::TouchHandlingResult
FilteredGestureProvider::OnTouchEvent(const MotionEvent& event) {
  DCHECK(!handling_event_);
  base::AutoReset<bool> handling_event(&handling_event_, true);

  pending_gesture_packet_ = GestureEventDataPacket::FromTouch(event);

  if (event.GetAction() == MotionEvent::Action::DOWN)
    any_touch_moved_beyond_slop_region_ = false;

  if (!gesture_provider_->OnTouchEvent(event))
    return TouchHandlingResult();

  TouchDispositionGestureFilter::PacketResult filter_result =
      gesture_filter_.OnGesturePacket(pending_gesture_packet_);
  if (filter_result != TouchDispositionGestureFilter::SUCCESS) {
    NOTREACHED() << "Invalid touch gesture sequence detected.";
    return TouchHandlingResult();
  }

  TouchHandlingResult result;
  result.succeeded = true;
  result.moved_beyond_slop_region = any_touch_moved_beyond_slop_region_;
  return result;
}

void FilteredGestureProvider::OnTouchEventAck(
    uint32_t unique_event_id,
    bool event_consumed,
    bool is_source_touch_event_set_non_blocking) {
  gesture_filter_.OnTouchEventAck(unique_event_id, event_consumed,
                                  is_source_touch_event_set_non_blocking);
}

void FilteredGestureProvider::ResetGestureHandlingState() {
  gesture_filter_.ResetGestureHandlingState();
}

void FilteredGestureProvider::ResetDetection() {
  gesture_provider_->ResetDetection();
}

void FilteredGestureProvider::SetMultiTouchZoomSupportEnabled(
    bool enabled) {
  gesture_provider_->SetMultiTouchZoomSupportEnabled(enabled);
}

void FilteredGestureProvider::SetDoubleTapSupportForPlatformEnabled(
    bool enabled) {
  gesture_provider_->SetDoubleTapSupportForPlatformEnabled(enabled);
}

void FilteredGestureProvider::SetDoubleTapSupportForPageEnabled(bool enabled) {
  gesture_provider_->SetDoubleTapSupportForPageEnabled(enabled);
}

const ui::MotionEvent* FilteredGestureProvider::GetCurrentDownEvent() const {
  return gesture_provider_->current_down_event();
}

void FilteredGestureProvider::OnGestureEvent(const GestureEventData& event) {
  if (handling_event_) {
    if (event.details.type() == ui::ET_GESTURE_SCROLL_BEGIN)
      any_touch_moved_beyond_slop_region_ = true;

    pending_gesture_packet_.Push(event);
    return;
  }

  gesture_filter_.OnGesturePacket(
      GestureEventDataPacket::FromTouchTimeout(event));
}

bool FilteredGestureProvider::RequiresDoubleTapGestureEvents() const {
  return client_->RequiresDoubleTapGestureEvents();
}

void FilteredGestureProvider::ForwardGestureEvent(
    const GestureEventData& event) {
  client_->OnGestureEvent(event);
}

}  // namespace ui
