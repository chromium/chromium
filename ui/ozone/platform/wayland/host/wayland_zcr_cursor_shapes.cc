// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_zcr_cursor_shapes.h"

#include <cursor-shapes-unstable-v1-client-protocol.h>

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
constexpr char WaylandZcrCursorShapes::kInterfaceName[];

// static
void WaylandZcrCursorShapes::Instantiate(WaylandConnection* connection,
                                         wl_registry* registry,
                                         uint32_t name,
                                         const std::string& interface,
                                         uint32_t version) {
  CHECK_EQ(interface, kInterfaceName) << "Expected \"" << kInterfaceName
                                      << "\" but got \"" << interface << "\"";

  if (connection->zcr_cursor_shapes_ ||
      !wl::CanBind(interface, version, kMinVersion, kMinVersion)) {
    return;
  }

  auto zcr_cursor_shapes =
      wl::Bind<zcr_cursor_shapes_v1>(registry, name, kMinVersion);
  if (!zcr_cursor_shapes) {
    LOG(ERROR) << "Failed to bind zcr_cursor_shapes_v1";
    return;
  }
  connection->zcr_cursor_shapes_ = std::make_unique<WaylandZcrCursorShapes>(
      zcr_cursor_shapes.release(), connection);
}

WaylandZcrCursorShapes::WaylandZcrCursorShapes(
    zcr_cursor_shapes_v1* zcr_cursor_shapes,
    WaylandConnection* connection)
    : zcr_cursor_shapes_v1_(zcr_cursor_shapes), connection_(connection) {
  // |zcr_cursor_shapes_v1_| and |connection_| may be null in tests.
}

WaylandZcrCursorShapes::~WaylandZcrCursorShapes() = default;

// static
std::optional<int32_t> WaylandZcrCursorShapes::ShapeFromType(CursorType type) {
  switch (type) {
    case CursorType::kNull:
      // kNull is an alias for kPointer. Fall through.
    case CursorType::kPointer:
      return ZCR_CURSOR_SHAPES_V1_CURSOR_SHAPE_TYPE_POINTER;
    case CursorType::kCross:
      return ZCR_CURSOR_SHAPES_V1_CURSOR_SHAPE_TYPE_CROSS;
    case CursorType::kHand:
      return ZCR_CURSOR_SHAPES_V1_CURSOR_SHAPE_TYPE_HAND;
    case CursorType::kIBeam:
      return ZCR_CURSOR_SHAPES_V1_CURSOR_SHAPE_TYPE_IBEAM;
    case CursorType::kWait:
      return ZCR_CURSOR_SHAPES_V1_CURSOR_SHAPE_TYPE_WAIT;
    case CursorType::kHelp:
      return ZCR_CURSOR_SHAPES_V1_CURSOR_SHAPE_TYPE_HELP;
    case CursorType::kEastResize:
      return ZCR_CURSOR_SHAPES_V1_CURSOR_SHAPE_TYPE_EAST_RESIZE;
    case CursorType::kNorthResize:
      return ZCR_CURSOR_SHAPES_V1_CURSOR_SHAPE_TYPE_NORTH_RESIZE;
    case CursorType::kNorthEastResize:
      return ZCR_CURSOR_SHAPES_V1_CURSOR_SHAPE_TYPE_NORTH_EAST_RESIZE;
    case CursorType::kNorthWestResize:
      return ZCR_CURSOR_SHAPES_V1_CURSOR_SHAPE_TYPE_NORTH_WEST_RESIZE;
    case CursorType::kSouthResize:
      return ZCR_CURSOR_SHAPES_V1_CURSOR_SHAPE_TYPE_SOUTH_RESIZE;
    case CursorType::kSouthEastResize:
      return ZCR_CURSOR_SHAPES_V1_CURSOR_SHAPE_TYPE_SOUTH_EAST_RESIZE;
    case CursorType::kSouthWestResize:
      return ZCR_CURSOR_SHAPES_V1_CURSOR_SHAPE_TYPE_SOUTH_WEST_RESIZE;
    case CursorType::kWestResize:
      return ZCR_CURSOR_SHAPES_V1_CURSOR_SHAPE_TYPE_WEST_RESIZE;
    case CursorType::kNorthSouthResize:
      return ZCR_CURSOR_SHAPES_V1_CURSOR_SHAPE_TYPE_NORTH_SOUTH_RESIZE;
    case CursorType::kEastWestResize:
      return ZCR_CURSOR_SHAPES_V1_CURSOR_SHAPE_TYPE_EAST_WEST_RESIZE;
    case CursorType::kNorthEastSouthWestResize:
      return ZCR_CURSOR_SHAPES_V1_CURSOR_SHAPE_TYPE_NORTH_EAST_SOUTH_WEST_RESIZE;
    case CursorType::kNorthWestSouthEastResize:
      return ZCR_CURSOR_SHAPES_V1_CURSOR_SHAPE_TYPE_NORTH_WEST_SOUTH_EAST_RESIZE;
    case CursorType::kColumnResize:
      return ZCR_CURSOR_SHAPES_V1_CURSOR_SHAPE_TYPE_COLUMN_RESIZE;
    case CursorType::kRowResize:
      return ZCR_CURSOR_SHAPES_V1_CURSOR_SHAPE_TYPE_ROW_RESIZE;
    case CursorType::kMiddlePanning:
      return ZCR_CURSOR_SHAPES_V1_CURSOR_SHAPE_TYPE_MIDDLE_PANNING;
    case CursorType::kEastPanning:
      return ZCR_CURSOR_SHAPES_V1_CURSOR_SHAPE_TYPE_EAST_PANNING;
    case CursorType::kNorthPanning:
      return ZCR_CURSOR_SHAPES_V1_CURSOR_SHAPE_TYPE_NORTH_PANNING;
    case CursorType::kNorthEastPanning:
      return ZCR_CURSOR_SHAPES_V1_CURSOR_SHAPE_TYPE_NORTH_EAST_PANNING;
    case CursorType::kNorthWestPanning:
      return ZCR_CURSOR_SHAPES_V1_CURSOR_SHAPE_TYPE_NORTH_WEST_PANNING;
    case CursorType::kSouthPanning:
      return ZCR_CURSOR_SHAPES_V1_CURSOR_SHAPE_TYPE_SOUTH_PANNING;
    case CursorType::kSouthEastPanning:
      return ZCR_CURSOR_SHAPES_V1_CURSOR_SHAPE_TYPE_SOUTH_EAST_PANNING;
    case CursorType::kSouthWestPanning:
      return ZCR_CURSOR_SHAPES_V1_CURSOR_SHAPE_TYPE_SOUTH_WEST_PANNING;
    case CursorType::kWestPanning:
      return ZCR_CURSOR_SHAPES_V1_CURSOR_SHAPE_TYPE_WEST_PANNING;
    case CursorType::kMove:
      return ZCR_CURSOR_SHAPES_V1_CURSOR_SHAPE_TYPE_MOVE;
    case CursorType::kVerticalText:
      return ZCR_CURSOR_SHAPES_V1_CURSOR_SHAPE_TYPE_VERTICAL_TEXT;
    case CursorType::kCell:
      return ZCR_CURSOR_SHAPES_V1_CURSOR_SHAPE_TYPE_CELL;
    case CursorType::kContextMenu:
      return ZCR_CURSOR_SHAPES_V1_CURSOR_SHAPE_TYPE_CONTEXT_MENU;
    case CursorType::kAlias:
      return ZCR_CURSOR_SHAPES_V1_CURSOR_SHAPE_TYPE_ALIAS;
    case CursorType::kProgress:
      return ZCR_CURSOR_SHAPES_V1_CURSOR_SHAPE_TYPE_PROGRESS;
    case CursorType::kNoDrop:
      return ZCR_CURSOR_SHAPES_V1_CURSOR_SHAPE_TYPE_NO_DROP;
    case CursorType::kCopy:
      return ZCR_CURSOR_SHAPES_V1_CURSOR_SHAPE_TYPE_COPY;
    case CursorType::kNone:
      return ZCR_CURSOR_SHAPES_V1_CURSOR_SHAPE_TYPE_NONE;
    case CursorType::kNotAllowed:
      return ZCR_CURSOR_SHAPES_V1_CURSOR_SHAPE_TYPE_NOT_ALLOWED;
    case CursorType::kZoomIn:
      return ZCR_CURSOR_SHAPES_V1_CURSOR_SHAPE_TYPE_ZOOM_IN;
    case CursorType::kZoomOut:
      return ZCR_CURSOR_SHAPES_V1_CURSOR_SHAPE_TYPE_ZOOM_OUT;
    case CursorType::kGrab:
      return ZCR_CURSOR_SHAPES_V1_CURSOR_SHAPE_TYPE_GRAB;
    case CursorType::kGrabbing:
      return ZCR_CURSOR_SHAPES_V1_CURSOR_SHAPE_TYPE_GRABBING;
    case CursorType::kMiddlePanningVertical:
    case CursorType::kMiddlePanningHorizontal:
    case CursorType::kEastWestNoResize:
    case CursorType::kNorthEastSouthWestNoResize:
    case CursorType::kNorthSouthNoResize:
    case CursorType::kNorthWestSouthEastNoResize:
      // Not supported by this API.
      return std::nullopt;
    case CursorType::kCustom:
      // Custom means a bitmap cursor, which can't use the shape API.
      return std::nullopt;
    case CursorType::kDndNone:
      return ZCR_CURSOR_SHAPES_V1_CURSOR_SHAPE_TYPE_DND_NONE;
    case CursorType::kDndMove:
      return ZCR_CURSOR_SHAPES_V1_CURSOR_SHAPE_TYPE_DND_MOVE;
    case CursorType::kDndCopy:
      return ZCR_CURSOR_SHAPES_V1_CURSOR_SHAPE_TYPE_DND_COPY;
    case CursorType::kDndLink:
      return ZCR_CURSOR_SHAPES_V1_CURSOR_SHAPE_TYPE_DND_LINK;
  }
}

void WaylandZcrCursorShapes::SetCursorShape(int32_t shape) {
  // Nothing to do if there's no pointer (mouse) connected.
  if (!connection_->seat()->pointer())
    return;
  zcr_cursor_shapes_v1_set_cursor_shape(
      zcr_cursor_shapes_v1_.get(), connection_->seat()->pointer()->wl_object(),
      shape);
}

}  // namespace ui
