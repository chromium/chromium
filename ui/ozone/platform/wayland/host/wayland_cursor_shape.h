// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_CURSOR_SHAPE_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_CURSOR_SHAPE_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-forward.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"

namespace ui {

class WaylandConnection;

// Wraps the cursor_shape interface for Wayland server-side cursor support.
class WaylandCursorShape
    : public wl::GlobalObjectRegistrar<WaylandCursorShape> {
 public:
  static constexpr char kInterfaceName[] = "wp_cursor_shape_manager_v1";

  static void Instantiate(WaylandConnection* connection,
                          wl_registry* registry,
                          uint32_t name,
                          const std::string& interface,
                          uint32_t version);

  WaylandCursorShape(wp_cursor_shape_manager_v1* cursor_shape,
                     WaylandConnection* connection);
  WaylandCursorShape(const WaylandCursorShape&) = delete;
  WaylandCursorShape& operator=(const WaylandCursorShape&) = delete;
  virtual ~WaylandCursorShape();

  // Returns the cursor shape value for a cursor |type|, or nullopt if the
  // type isn't supported by Wayland's cursor shape API.
  static std::optional<uint32_t> ShapeFromType(mojom::CursorType type);

  // Calls wp_cursor_shape_device_v1_set_shape(). See interface description
  // for values for |shape|. Virtual for testing.
  virtual void SetCursorShape(uint32_t shape);

 private:
  wp_cursor_shape_device_v1* GetShapeDevice();

  const wl::Object<wp_cursor_shape_manager_v1> wp_cursor_shape_manager_v1_;
  wl::Object<wp_cursor_shape_device_v1> wp_cursor_shape_device_v1_;
  const raw_ptr<WaylandConnection> connection_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_CURSOR_SHAPE_H_
