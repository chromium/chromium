// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_WIN_STYLUS_HANDWRITING_PROPERTIES_WIN_H_
#define UI_EVENTS_WIN_STYLUS_HANDWRITING_PROPERTIES_WIN_H_

#include <cstdint>
#include <optional>

#include "ui/events/event.h"
#include "ui/events/events_export.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace ui {

// Structure to hold stylus handwriting specific details. This is only intended
// to be used in the Browser process.
struct EVENTS_EXPORT StylusHandwritingPropertiesWin {
  StylusHandwritingPropertiesWin();
  StylusHandwritingPropertiesWin(uint32_t handwriting_pointer_id,
                                 uint64_t handwriting_stroke_id);
  StylusHandwritingPropertiesWin(uint32_t handwriting_pointer_id,
                                 uint64_t handwriting_stroke_id,
                                 std::optional<gfx::Vector2dF> pixels_per_inch);
  // The pointer id supplied by the OS which is used for communication with
  // the Shell Handwriting API. The handwriting pointer id is different from
  // `PointerDetails.id` which is a generated id created by
  // SequentialIDGenerator.
  uint32_t handwriting_pointer_id = 0;
  // The handwriting stroke id supplied by the OS based on the pointer id.
  // It is used for communication with the Shell Handwriting API. Assigned on
  // WM_POINTERDOWN.
  uint64_t handwriting_stroke_id = 0;
  // The initiating pointer device's pixels per inch.
  // See display::win::ScreenWin::GetPixelsPerInchForPointerDevice().
  gfx::Vector2dF pixels_per_inch;
};

// Returns keys used to store handwriting pointer and stroke ids in the event
// properties. The values should not be used outside the test environment.
// Please use SetStylusHandwritingProperties().
EVENTS_EXPORT const char* GetPropertyHandwritingPointerIdKeyForTesting();
EVENTS_EXPORT const char* GetPropertyHandwritingStrokeIdKeyForTesting();

// Creates event properties for testing with the provided stylus handwriting
// properties.
EVENTS_EXPORT Event::Properties CreateEventPropertiesForTesting(
    const StylusHandwritingPropertiesWin& properties);

// Sets stylus handwriting properties (stroke id, pointer id, and pixels per
// inch) to the provided event.
EVENTS_EXPORT void SetStylusHandwritingProperties(
    Event& event,
    const StylusHandwritingPropertiesWin& properties);

// Gets stylus handwriting properties (stroke id, pointer id, and pixels per
// inch) if such exist from the provided event.
EVENTS_EXPORT std::optional<StylusHandwritingPropertiesWin>
GetStylusHandwritingProperties(const Event& event);

// Gets the handwriting stroke id from the OS based on the pointer id. The
// handwriting stroke id is used for communication with the Shell Handwriting
// API.
EVENTS_EXPORT uint64_t GetHandwritingStrokeId(uint32_t pointer_id);

}  // namespace ui

#endif  // UI_EVENTS_WIN_STYLUS_HANDWRITING_PROPERTIES_WIN_H_
