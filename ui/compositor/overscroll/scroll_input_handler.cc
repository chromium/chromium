// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/overscroll/scroll_input_handler.h"

#include "ui/compositor/layer.h"
#include "ui/events/event.h"
#include "ui/events/types/scroll_input_type.h"

namespace ui {

namespace {

// Creates a cc::ScrollState from a ui::ScrollEvent, populating fields general
// to all event phases. Take care not to put deltas on beginning/end updates,
// since InputHandler will DCHECK if they're present.
cc::ScrollState CreateScrollState(const ScrollEvent& scroll, bool is_begin) {
  cc::ScrollStateData scroll_state_data;
  scroll_state_data.position_x = scroll.x();
  scroll_state_data.position_y = scroll.y();
  if (is_begin) {
    scroll_state_data.delta_x_hint = -scroll.x_offset_ordinal();
    scroll_state_data.delta_y_hint = -scroll.y_offset_ordinal();
  } else {
    scroll_state_data.delta_x = -scroll.x_offset_ordinal();
    scroll_state_data.delta_y = -scroll.y_offset_ordinal();
  }

  scroll_state_data.is_in_inertial_phase =
      scroll.momentum_phase() == EventMomentumPhase::INERTIAL_UPDATE;
  scroll_state_data.is_ending = false;
  scroll_state_data.is_beginning = is_begin;
  return cc::ScrollState(scroll_state_data);
}

}  // namespace

ScrollInputHandler::ScrollInputHandler(
    const base::WeakPtr<cc::InputHandler>& input_handler)
    : input_handler_weak_ptr_(input_handler) {
  DCHECK(input_handler_weak_ptr_);
  input_handler_weak_ptr_->BindToClient(this);
}

ScrollInputHandler::~ScrollInputHandler() {
  DCHECK(!input_handler_weak_ptr_)
      << "Pointer invalidated before WillShutdown() is called.";
}

bool ScrollInputHandler::OnScrollEvent(const ScrollEvent& event,
                                       Layer* layer_to_scroll) {
  if (!input_handler_weak_ptr_)
    return false;

  // TODO(bokan): This code would ideally call Begin once at the beginning of a
  // gesture and End once at the end of it. Unfortunately, the phase data is
  // incomplete (only momentum phase is set and in a reduced way, e.g. we get
  // an end when we transition to a fling). See the TODOs in events_mac.mm and
  // ui/events/event.cc. Until those are fixed, there's no harm in simply
  // treating each event as a gesture unto itself.
  cc::ScrollState scroll_state_begin = CreateScrollState(event, true);
  scroll_state_begin.data()->set_current_native_scrolling_element(
      layer_to_scroll->element_id());

  // Note: the WHEEL type covers both actual wheels as well as trackpad
  // scrolling.
  cc::InputHandler::ScrollStatus result = input_handler_weak_ptr_->ScrollBegin(
      &scroll_state_begin, ui::ScrollInputType::kWheel);

  // Falling back to the main thread should never be required when an explicit
  // ElementId is provided.
  DCHECK(!result.main_thread_hit_test_reasons);

  input_handler_weak_ptr_->ScrollUpdate(CreateScrollState(event, false),
                                        base::TimeDelta());
  input_handler_weak_ptr_->ScrollEnd(/*should_snap=*/false);

  return true;
}

void ScrollInputHandler::WillShutdown() {
  DCHECK(input_handler_weak_ptr_);
  input_handler_weak_ptr_.reset();
}

void ScrollInputHandler::Animate(base::TimeTicks time) {}

void ScrollInputHandler::ReconcileElasticOverscrollAndRootScroll() {}

void ScrollInputHandler::SetPrefersReducedMotion(bool prefers_reduced_motion) {}

void ScrollInputHandler::UpdateRootLayerStateForSynchronousInputHandler(
    const gfx::PointF& total_scroll_offset,
    const gfx::PointF& max_scroll_offset,
    const gfx::SizeF& scrollable_size,
    float page_scale_factor,
    float min_page_scale_factor,
    float max_page_scale_factor) {}

void ScrollInputHandler::DeliverInputForBeginFrame(
    const viz::BeginFrameArgs& args) {}
void ScrollInputHandler::DeliverInputForHighLatencyMode() {}
void ScrollInputHandler::DeliverInputForDeadline() {}
void ScrollInputHandler::DidFinishImplFrame() {}
bool ScrollInputHandler::HasQueuedInput() const {
  return false;
}
void ScrollInputHandler::SetScrollEventDispatchMode(
    cc::InputHandlerClient::ScrollEventDispatchMode mode) {}

}  // namespace ui
