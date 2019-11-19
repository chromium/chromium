// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/overscroll/scroll_input_handler.h"

#include "ui/compositor/layer.h"
#include "ui/events/event.h"

namespace ui {

namespace {

// Creates a cc::ScrollState from a ui::ScrollEvent, populating fields general
// to all event phases. Take care not to put deltas on beginning/end updates,
// since InputHandler will DCHECK if they're present.
cc::ScrollState CreateScrollState(const ScrollEvent& scroll, bool is_end) {
  cc::ScrollStateData scroll_state_data;
  scroll_state_data.position_x = scroll.x();
  scroll_state_data.position_y = scroll.y();
  if (!is_end) {
    scroll_state_data.delta_x = -scroll.x_offset_ordinal();
    scroll_state_data.delta_y = -scroll.y_offset_ordinal();
  }

  scroll_state_data.is_in_inertial_phase =
      scroll.momentum_phase() == EventMomentumPhase::INERTIAL_UPDATE;
  scroll_state_data.is_ending = is_end;
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

  cc::ScrollState scroll_state = CreateScrollState(event, false);
  scroll_state.data()->set_current_native_scrolling_element(
      layer_to_scroll->element_id());
  input_handler_weak_ptr_->ScrollBy(&scroll_state);

  if (event.momentum_phase() == EventMomentumPhase::END) {
    scroll_state = CreateScrollState(event, true);

    // For now, pass false for the |should_snap| argument.
    input_handler_weak_ptr_->ScrollEnd(&scroll_state, false /* should_snap */);
  }

  return true;
}

void ScrollInputHandler::WillShutdown() {
  DCHECK(input_handler_weak_ptr_);
  input_handler_weak_ptr_.reset();
}

void ScrollInputHandler::Animate(base::TimeTicks time) {}

void ScrollInputHandler::ReconcileElasticOverscrollAndRootScroll() {}

void ScrollInputHandler::UpdateRootLayerStateForSynchronousInputHandler(
    const gfx::ScrollOffset& total_scroll_offset,
    const gfx::ScrollOffset& max_scroll_offset,
    const gfx::SizeF& scrollable_size,
    float page_scale_factor,
    float min_page_scale_factor,
    float max_page_scale_factor) {}

void ScrollInputHandler::DeliverInputForBeginFrame(
    const viz::BeginFrameArgs& args) {}
void ScrollInputHandler::DeliverInputForHighLatencyMode() {}

}  // namespace ui
