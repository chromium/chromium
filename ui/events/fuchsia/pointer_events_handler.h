// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_FUCHSIA_POINTER_EVENTS_HANDLER_H_
#define UI_EVENTS_FUCHSIA_POINTER_EVENTS_HANDLER_H_

#include <fidl/fuchsia.ui.pointer/cpp/fidl.h>

#include <array>
#include <functional>
#include <optional>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "ui/events/event.h"
#include "ui/events/events_export.h"

namespace ui {

// Helper class for keying into a map.
struct InteractionLess {
  bool operator()(const fuchsia_ui_pointer::TouchInteractionId& interaction_id,
                  const fuchsia_ui_pointer::TouchInteractionId&
                      other_interaction_id) const {
    return (std::hash<uint32_t>()(interaction_id.device_id()) ^
            std::hash<uint32_t>()(interaction_id.pointer_id()) ^
            std::hash<uint32_t>()(interaction_id.interaction_id())) <
           (std::hash<uint32_t>()(other_interaction_id.device_id()) ^
            std::hash<uint32_t>()(other_interaction_id.pointer_id()) ^
            std::hash<uint32_t>()(other_interaction_id.interaction_id()));
  }
};

// Channel processors for fuchsia.ui.pointer.TouchSource and MouseSource
// protocols. It manages the channel state, collects touch and mouse events, and
// surfaces them to FlatlandWindow as ui::Event events for further processing
// and dispatch. ui::Events have logical coordinates and they might be scaled by
// the view pixel ratio in FlatlandWindow to get physical coordinates.
class EVENTS_EXPORT PointerEventsHandler {
 public:
  PointerEventsHandler(
      fidl::ClientEnd<fuchsia_ui_pointer::TouchSource> touch_source,
      fidl::ClientEnd<fuchsia_ui_pointer::MouseSource> mouse_source);
  ~PointerEventsHandler();

  // This function collects Fuchsia's TouchPointerSample and MousePointerSample
  // data and transforms them into ui::Events. It then calls the supplied
  // callback with a vector of ui::Events, which does last processing
  // (applies metrics).
  void StartWatching(base::RepeatingCallback<void(Event*)> event_callback);

 private:
  using MouseDeviceId = uint32_t;

  void OnTouchSourceWatchResult(
      fidl::Result<fuchsia_ui_pointer::TouchSource::Watch>& result);
  void OnMouseSourceWatchResult(
      fidl::Result<fuchsia_ui_pointer::MouseSource::Watch>& result);

  base::RepeatingCallback<void(Event*)> event_callback_;

  // Touch State ---------------------------------------------------------------
  // Channel for touch events from Scenic.
  fidl::Client<fuchsia_ui_pointer::TouchSource> touch_source_;

  // Receive touch events from Scenic. Must be copyable.
  fit::function<void(std::vector<fuchsia_ui_pointer::TouchEvent>)>
      touch_responder_;

  // Per-interaction buffer of touch events from Scenic. When an interaction
  // starts with event.pointer_sample.phase == ADD, we allocate a buffer and
  // store samples. When interaction ownership becomes
  // event.interaction_result.status == GRANTED, we flush the buffer to client,
  // delete the buffer, and all future events in this interaction are flushed
  // direct to client. When interaction ownership becomes DENIED, we delete the
  // buffer, and the client does not get any previous or future events in this
  // interaction.
  base::flat_map<fuchsia_ui_pointer::TouchInteractionId,
                 std::vector<TouchEvent>,
                 InteractionLess>
      touch_buffer_;

  // The fuchsia.ui.pointer.TouchSource protocol allows one in-flight
  // hanging-get Watch() call to gather touch events, and the client is expected
  // to respond with consumption intent on the following hanging-get Watch()
  // call. Store responses here for the next call.
  std::vector<fuchsia_ui_pointer::TouchResponse> touch_responses_;

  // The fuchsia.ui.pointer.TouchSource protocol issues channel-global view
  // parameters on connection and on change. Events must apply these view
  // parameters to correctly map to logical view coordinates. The "nullopt"
  // state represents the absence of view parameters, early in the protocol
  // lifecycle.
  std::optional<fuchsia_ui_pointer::ViewParameters> touch_view_parameters_;

  // Mouse State ---------------------------------------------------------------
  // Channel for mouse events from Scenic.
  fidl::Client<fuchsia_ui_pointer::MouseSource> mouse_source_;

  // Receive mouse events from Scenic. Must be copyable.
  fit::function<void(std::vector<fuchsia_ui_pointer::MouseEvent>)>
      mouse_responder_;

  // Each MouseDeviceId maps to the bitmap of currently pressed buttons for that
  // mouse. For example, if the 11th bit is set in a given |mouse_down[id]|,
  // then the left mouse button is pressed, since EF_LEFT_MOUSE_BUTTON == 1 <<
  // 11, as defined in ui/events/event_constants.h.
  //
  // The high level algorithm for any given mouse and button is:
  //   if !mouse_down[id] && !button then: change = EventType::kMouseMoved
  //   if !mouse_down[id] &&  button then: change = EventType::kMousePressed;
  //       mouse_down[id] |= button // sets button bit to 1
  //   if  mouse_down[id] &&  button then: change = EventType::kMouseDragged
  //   if  mouse_down[id] && !button then: change = EventType::kMouseReleased;
  //       mouse_down[id] ^= button // sets button bit to 0
  base::flat_map<MouseDeviceId, /*pressed_buttons_flags=*/int> mouse_down_;

  // For each mouse device, its device-specific information, such as mouse
  // button priority order.
  base::flat_map<MouseDeviceId, fuchsia_ui_pointer::MouseDeviceInfo>
      mouse_device_info_;

  // The fuchsia.ui.pointer.MouseSource protocol issues channel-global view
  // parameters on connection and on change. Events must apply these view
  // parameters to correctly map to logical view coordinates. The "nullopt"
  // state represents the absence of view parameters, early in the protocol
  // lifecycle.
  std::optional<fuchsia_ui_pointer::ViewParameters> mouse_view_parameters_;
};

}  // namespace ui

#endif  // UI_EVENTS_FUCHSIA_POINTER_EVENTS_HANDLER_H_
