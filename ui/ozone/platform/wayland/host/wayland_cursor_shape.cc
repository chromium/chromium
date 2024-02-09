// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_cursor_shape.h"

#include <cursor-shape-v1-client-protocol.h>

#include <optional>

#include "base/check.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_pointer.h"
#include "ui/ozone/platform/wayland/host/wayland_seat.h"

namespace ui {

namespace {
constexpr uint32_t kMinVersion = 1;
}

using mojom::CursorType;

// static
constexpr char WaylandCursorShape::kInterfaceName[];

// static
void WaylandCursorShape::Instantiate(WaylandConnection* connection,
                                     wl_registry* registry,
                                     uint32_t name,
                                     const std::string& interface,
                                     uint32_t version) {
  CHECK_EQ(interface, kInterfaceName) << "Expected \"" << kInterfaceName
                                      << "\" but got \"" << interface << "\"";

  if (connection->cursor_shape_ ||
      !wl::CanBind(interface, version, kMinVersion, kMinVersion)) {
    return;
  }

  auto cursor_shape_manager =
      wl::Bind<wp_cursor_shape_manager_v1>(registry, name, kMinVersion);
  if (!cursor_shape_manager) {
    LOG(ERROR) << "Failed to bind wp_cursor_shape_manager_v1";
    return;
  }
  connection->cursor_shape_ = std::make_unique<WaylandCursorShape>(
      cursor_shape_manager.release(), connection);
}

WaylandCursorShape::WaylandCursorShape(wp_cursor_shape_manager_v1* cursor_shape,
                                       WaylandConnection* connection)
    : wp_cursor_shape_manager_v1_(cursor_shape), connection_(connection) {
  // |wp_cursor_shape_manager_v1_| and |connection_| may be null in tests.
}

WaylandCursorShape::~WaylandCursorShape() = default;

wp_cursor_shape_device_v1* WaylandCursorShape::GetShapeDevice() {
  DCHECK(connection_->seat()->pointer());

  if (!wp_cursor_shape_device_v1_.get()) {
    wl_pointer* pointer = connection_->seat()->pointer()->wl_object();
    wp_cursor_shape_device_v1_.reset(wp_cursor_shape_manager_v1_get_pointer(
        wp_cursor_shape_manager_v1_.get(), pointer));
  }
  DCHECK(wp_cursor_shape_device_v1_);
  return wp_cursor_shape_device_v1_.get();
}

// static
std::optional<uint32_t> WaylandCursorShape::ShapeFromType(CursorType type) {
  switch (type) {
    case CursorType::kNull:
      // kNull is an alias for kPointer. Fall through.
    case CursorType::kPointer:
      return WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_DEFAULT;
    case CursorType::kCross:
      return WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_CROSSHAIR;
    case CursorType::kHand:
      return WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_POINTER;
    case CursorType::kIBeam:
      return WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_TEXT;
    case CursorType::kWait:
      return WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_WAIT;
    case CursorType::kHelp:
      return WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_HELP;
    case CursorType::kEastResize:
    case CursorType::kEastPanning:
      return WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_E_RESIZE;
    case CursorType::kNorthResize:
    case CursorType::kNorthPanning:
      return WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_N_RESIZE;
    case CursorType::kNorthEastResize:
    case CursorType::kNorthEastPanning:
      return WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_NE_RESIZE;
    case CursorType::kNorthWestResize:
    case CursorType::kNorthWestPanning:
      return WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_NW_RESIZE;
    case CursorType::kSouthResize:
    case CursorType::kSouthPanning:
      return WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_S_RESIZE;
    case CursorType::kSouthEastResize:
    case CursorType::kSouthEastPanning:
      return WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_SE_RESIZE;
    case CursorType::kSouthWestResize:
    case CursorType::kSouthWestPanning:
      return WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_SW_RESIZE;
    case CursorType::kWestResize:
    case CursorType::kWestPanning:
      return WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_W_RESIZE;
    case CursorType::kNorthSouthResize:
      return WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_NS_RESIZE;
    case CursorType::kEastWestResize:
      return WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_EW_RESIZE;
    case CursorType::kNorthEastSouthWestResize:
      return WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_NESW_RESIZE;
    case CursorType::kNorthWestSouthEastResize:
      return WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_NWSE_RESIZE;
    case CursorType::kColumnResize:
      return WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_COL_RESIZE;
    case CursorType::kRowResize:
      return WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_ROW_RESIZE;
    case CursorType::kMove:
      // Returning `MOVE` is the correct thing here, but Blink does not make a
      // distinction between move and all-scroll. Other platforms use a cursor
      // more consistent with all-scroll, so use that.
    case CursorType::kMiddlePanning:
    case CursorType::kMiddlePanningVertical:
    case CursorType::kMiddlePanningHorizontal:
      return WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_ALL_SCROLL;
    case CursorType::kVerticalText:
      return WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_VERTICAL_TEXT;
    case CursorType::kCell:
      return WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_CELL;
    case CursorType::kContextMenu:
      return WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_CONTEXT_MENU;
    case CursorType::kAlias:
      return WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_ALIAS;
    case CursorType::kProgress:
      return WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_PROGRESS;
    case CursorType::kNoDrop:
      return WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_NO_DROP;
    case CursorType::kCopy:
    case CursorType::kDndCopy:
      return WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_COPY;
    case CursorType::kNone:
      // To be cleared through wl_pointer.set_cursor.
      return std::nullopt;
    case CursorType::kNotAllowed:
    case CursorType::kNorthSouthNoResize:
    case CursorType::kEastWestNoResize:
    case CursorType::kNorthEastSouthWestNoResize:
    case CursorType::kNorthWestSouthEastNoResize:
      return WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_NOT_ALLOWED;
    case CursorType::kZoomIn:
      return WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_ZOOM_IN;
    case CursorType::kZoomOut:
      return WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_ZOOM_OUT;
    case CursorType::kGrab:
      return WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_GRAB;
    case CursorType::kGrabbing:
    case CursorType::kDndNone:
    case CursorType::kDndMove:
    case CursorType::kDndLink:
      // For drag-and-drop, the compositor knows the drag type and can use it to
      // additionally decorate the cursor.
      return WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_GRABBING;
    case CursorType::kCustom:
      // "Custom" means a bitmap cursor, which cannot use the shape API.
      return std::nullopt;
  }
}

void WaylandCursorShape::SetCursorShape(uint32_t shape) {
  DCHECK(connection_->seat());

  // Nothing to do if there is no pointer (mouse) connected.
  if (!connection_->seat()->pointer()) {
    return;
  }

  auto pointer_enter_serial =
      connection_->serial_tracker().GetSerial(wl::SerialType::kMouseEnter);
  if (!pointer_enter_serial) {
    VLOG(1) << "Failed to set cursor shape: no mouse enter serial found.";
    return;
  }
  wp_cursor_shape_device_v1_set_shape(GetShapeDevice(),
                                      pointer_enter_serial->value, shape);
}

}  // namespace ui
