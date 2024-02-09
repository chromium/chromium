// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_ZCR_CURSOR_SHAPES_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_ZCR_CURSOR_SHAPES_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-forward.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"

namespace ui {

class WaylandConnection;

// Wraps the zcr_cursor_shapes interface for Wayland (exo) server-side cursor
// support. Exists to support Lacros, which uses server-side cursors for
// consistency with ARC++ and for accessibility support.
class WaylandZcrCursorShapes
    : public wl::GlobalObjectRegistrar<WaylandZcrCursorShapes> {
 public:
  static constexpr char kInterfaceName[] = "zcr_cursor_shapes_v1";

  static void Instantiate(WaylandConnection* connection,
                          wl_registry* registry,
                          uint32_t name,
                          const std::string& interface,
                          uint32_t version);

  WaylandZcrCursorShapes(zcr_cursor_shapes_v1* zcr_cursor_shapes,
                         WaylandConnection* connection);
  WaylandZcrCursorShapes(const WaylandZcrCursorShapes&) = delete;
  WaylandZcrCursorShapes& operator=(const WaylandZcrCursorShapes&) = delete;
  virtual ~WaylandZcrCursorShapes();

  // Returns the cursor shape value for a cursor |type|, or nullopt if the
  // type isn't supported by the cursor API.
  static std::optional<int32_t> ShapeFromType(mojom::CursorType type);

  // Calls zcr_cursor_shapes_v1_set_cursor_shape(). See interface description
  // for values for |shape|. Virtual for testing.
  virtual void SetCursorShape(int32_t shape);

 private:
  wl::Object<zcr_cursor_shapes_v1> zcr_cursor_shapes_v1_;
  const raw_ptr<WaylandConnection> connection_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_ZCR_CURSOR_SHAPES_H_
