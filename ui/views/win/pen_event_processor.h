// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIN_PEN_EVENT_PROCESSOR_H_
#define UI_VIEWS_WIN_PEN_EVENT_PROCESSOR_H_

#include <windows.h>

#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/sequential_id_generator.h"
#include "ui/views/views_export.h"
#include "ui/views/win/pen_id_handler.h"

namespace views {

// This class handles the processing pen event state information
// from native Windows events and returning appropriate
// ui::Events for the current state.
class VIEWS_EXPORT PenEventProcessor {
 public:
  // |id_generator| must outlive this object's lifecycle.
  PenEventProcessor(ui::SequentialIDGenerator* id_generator,
                    bool direct_manipulation_enabled);

  PenEventProcessor(const PenEventProcessor&) = delete;
  PenEventProcessor& operator=(const PenEventProcessor&) = delete;

  ~PenEventProcessor();

  // Generate an appropriate ui::Event for a given pen pointer.
  // May return nullptr if no event should be dispatched.
  std::unique_ptr<ui::Event> GenerateEvent(
      UINT message,
      UINT32 pointer_id,
      const POINTER_PEN_INFO& pen_pointer_info,
      const gfx::Point& point);

 private:
  std::unique_ptr<ui::Event> GenerateMouseEvent(
      UINT message,
      UINT32 pointer_id,
      const POINTER_INFO& pointer_info,
      const gfx::Point& point,
      const ui::PointerDetails& pointer_details,
      int32_t device_id);
  std::unique_ptr<ui::Event> GenerateTouchEvent(
      UINT message,
      UINT32 pointer_id,
      const POINTER_INFO& pointer_info,
      const gfx::Point& point,
      const ui::PointerDetails& pointer_details,
      int32_t device_id);

  raw_ptr<ui::SequentialIDGenerator> id_generator_;
  bool direct_manipulation_enabled_;
  base::flat_map<UINT32, bool> pen_in_contact_;
  base::flat_map<UINT32, bool> send_touch_for_pen_;

  // There may be more than one pen used at the same time.
  base::flat_map<UINT32, bool> sent_mouse_down_;
  base::flat_map<UINT32, bool> sent_touch_start_;

  std::optional<ui::PointerId> eraser_pointer_id_;

  PenIdHandler pen_id_handler_;
};

}  // namespace views

#endif  // UI_VIEWS_WIN_PEN_EVENT_PROCESSOR_H_
