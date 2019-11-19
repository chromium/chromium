// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_OVERSCROLL_SCROLL_INPUT_HANDLER_H_
#define UI_COMPOSITOR_OVERSCROLL_SCROLL_INPUT_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "cc/input/input_handler.h"
#include "ui/compositor/compositor_export.h"

namespace ui {

class Layer;
class ScrollEvent;

// Class to feed UI-thread scroll events to a cc::InputHandler. Inspired by
// ui::InputHandlerProxy but greatly simplified.
class COMPOSITOR_EXPORT ScrollInputHandler : public cc::InputHandlerClient {
 public:
  explicit ScrollInputHandler(
      const base::WeakPtr<cc::InputHandler>& input_handler);
  ~ScrollInputHandler() override;

  // Ask the InputHandler to scroll |element| according to |scroll|.
  bool OnScrollEvent(const ScrollEvent& event, Layer* layer_to_scroll);

  // cc::InputHandlerClient:
  void WillShutdown() override;
  void Animate(base::TimeTicks time) override;
  void ReconcileElasticOverscrollAndRootScroll() override;
  void UpdateRootLayerStateForSynchronousInputHandler(
      const gfx::ScrollOffset& total_scroll_offset,
      const gfx::ScrollOffset& max_scroll_offset,
      const gfx::SizeF& scrollable_size,
      float page_scale_factor,
      float min_page_scale_factor,
      float max_page_scale_factor) override;
  void DeliverInputForBeginFrame(const viz::BeginFrameArgs& args) override;
  void DeliverInputForHighLatencyMode() override;

 private:
  // Cleared in WillShutdown().
  base::WeakPtr<cc::InputHandler> input_handler_weak_ptr_;

  DISALLOW_COPY_AND_ASSIGN(ScrollInputHandler);
};

}  // namespace ui

#endif  // UI_COMPOSITOR_OVERSCROLL_UI_INPUT_HANDLER_H_
