// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file was automatically generated with:
// ../../ui/gfx/x/gen_xproto.py \
//    ../../third_party/xcbproto/src \
//    gen/ui/gfx/x \
//    bigreq \
//    dri3 \
//    glx \
//    randr \
//    render \
//    screensaver \
//    shape \
//    shm \
//    sync \
//    xfixes \
//    xinput \
//    xkb \
//    xproto \
//    xtest

#ifndef UI_GFX_X_GENERATED_PROTOS_SHAPE_H_
#define UI_GFX_X_GENERATED_PROTOS_SHAPE_H_

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <vector>

#include "base/component_export.h"
#include "base/files/scoped_file.h"
#include "base/memory/scoped_refptr.h"
#include "ui/gfx/x/error.h"
#include "ui/gfx/x/ref_counted_fd.h"
#include "ui/gfx/x/xproto_types.h"
#include "xproto.h"

namespace x11 {

class Connection;

template <typename Reply>
struct Response;

template <typename Reply>
class Future;

class COMPONENT_EXPORT(X11) Shape {
 public:
  static constexpr unsigned major_version = 1;
  static constexpr unsigned minor_version = 1;

  Shape(Connection* connection, const x11::QueryExtensionReply& info);

  uint8_t present() const { return info_.present; }
  uint8_t major_opcode() const { return info_.major_opcode; }
  uint8_t first_event() const { return info_.first_event; }
  uint8_t first_error() const { return info_.first_error; }

  Connection* connection() const { return connection_; }

  enum class Operation : uint8_t {};

  enum class Kind : uint8_t {};

  enum class So : int {
    Set = 0,
    Union = 1,
    Intersect = 2,
    Subtract = 3,
    Invert = 4,
  };

  enum class Sk : int {
    Bounding = 0,
    Clip = 1,
    Input = 2,
  };

  struct NotifyEvent {
    static constexpr uint8_t type_id = 6;
    static constexpr uint8_t opcode = 0;
    Sk shape_kind{};
    uint16_t sequence{};
    Window affected_window{};
    int16_t extents_x{};
    int16_t extents_y{};
    uint16_t extents_width{};
    uint16_t extents_height{};
    Time server_time{};
    uint8_t shaped{};
  };

  struct QueryVersionRequest {};

  struct QueryVersionReply {
    uint16_t sequence{};
    uint16_t major_version{};
    uint16_t minor_version{};
  };

  using QueryVersionResponse = Response<QueryVersionReply>;

  Future<QueryVersionReply> QueryVersion(const QueryVersionRequest& request);

  Future<QueryVersionReply> QueryVersion();

  struct RectanglesRequest {
    So operation{};
    Sk destination_kind{};
    ClipOrdering ordering{};
    Window destination_window{};
    int16_t x_offset{};
    int16_t y_offset{};
    std::vector<Rectangle> rectangles{};
  };

  using RectanglesResponse = Response<void>;

  Future<void> Rectangles(const RectanglesRequest& request);

  Future<void> Rectangles(const So& operation = {},
                          const Sk& destination_kind = {},
                          const ClipOrdering& ordering = {},
                          const Window& destination_window = {},
                          const int16_t& x_offset = {},
                          const int16_t& y_offset = {},
                          const std::vector<Rectangle>& rectangles = {});

  struct MaskRequest {
    So operation{};
    Sk destination_kind{};
    Window destination_window{};
    int16_t x_offset{};
    int16_t y_offset{};
    Pixmap source_bitmap{};
  };

  using MaskResponse = Response<void>;

  Future<void> Mask(const MaskRequest& request);

  Future<void> Mask(const So& operation = {},
                    const Sk& destination_kind = {},
                    const Window& destination_window = {},
                    const int16_t& x_offset = {},
                    const int16_t& y_offset = {},
                    const Pixmap& source_bitmap = {});

  struct CombineRequest {
    So operation{};
    Sk destination_kind{};
    Sk source_kind{};
    Window destination_window{};
    int16_t x_offset{};
    int16_t y_offset{};
    Window source_window{};
  };

  using CombineResponse = Response<void>;

  Future<void> Combine(const CombineRequest& request);

  Future<void> Combine(const So& operation = {},
                       const Sk& destination_kind = {},
                       const Sk& source_kind = {},
                       const Window& destination_window = {},
                       const int16_t& x_offset = {},
                       const int16_t& y_offset = {},
                       const Window& source_window = {});

  struct OffsetRequest {
    Sk destination_kind{};
    Window destination_window{};
    int16_t x_offset{};
    int16_t y_offset{};
  };

  using OffsetResponse = Response<void>;

  Future<void> Offset(const OffsetRequest& request);

  Future<void> Offset(const Sk& destination_kind = {},
                      const Window& destination_window = {},
                      const int16_t& x_offset = {},
                      const int16_t& y_offset = {});

  struct QueryExtentsRequest {
    Window destination_window{};
  };

  struct QueryExtentsReply {
    uint16_t sequence{};
    uint8_t bounding_shaped{};
    uint8_t clip_shaped{};
    int16_t bounding_shape_extents_x{};
    int16_t bounding_shape_extents_y{};
    uint16_t bounding_shape_extents_width{};
    uint16_t bounding_shape_extents_height{};
    int16_t clip_shape_extents_x{};
    int16_t clip_shape_extents_y{};
    uint16_t clip_shape_extents_width{};
    uint16_t clip_shape_extents_height{};
  };

  using QueryExtentsResponse = Response<QueryExtentsReply>;

  Future<QueryExtentsReply> QueryExtents(const QueryExtentsRequest& request);

  Future<QueryExtentsReply> QueryExtents(const Window& destination_window = {});

  struct SelectInputRequest {
    Window destination_window{};
    uint8_t enable{};
  };

  using SelectInputResponse = Response<void>;

  Future<void> SelectInput(const SelectInputRequest& request);

  Future<void> SelectInput(const Window& destination_window = {},
                           const uint8_t& enable = {});

  struct InputSelectedRequest {
    Window destination_window{};
  };

  struct InputSelectedReply {
    uint8_t enabled{};
    uint16_t sequence{};
  };

  using InputSelectedResponse = Response<InputSelectedReply>;

  Future<InputSelectedReply> InputSelected(const InputSelectedRequest& request);

  Future<InputSelectedReply> InputSelected(
      const Window& destination_window = {});

  struct GetRectanglesRequest {
    Window window{};
    Sk source_kind{};
  };

  struct GetRectanglesReply {
    ClipOrdering ordering{};
    uint16_t sequence{};
    std::vector<Rectangle> rectangles{};
  };

  using GetRectanglesResponse = Response<GetRectanglesReply>;

  Future<GetRectanglesReply> GetRectangles(const GetRectanglesRequest& request);

  Future<GetRectanglesReply> GetRectangles(const Window& window = {},
                                           const Sk& source_kind = {});

 private:
  Connection* const connection_;
  x11::QueryExtensionReply info_{};
};

}  // namespace x11

inline constexpr x11::Shape::So operator|(x11::Shape::So l, x11::Shape::So r) {
  using T = std::underlying_type_t<x11::Shape::So>;
  return static_cast<x11::Shape::So>(static_cast<T>(l) | static_cast<T>(r));
}

inline constexpr x11::Shape::So operator&(x11::Shape::So l, x11::Shape::So r) {
  using T = std::underlying_type_t<x11::Shape::So>;
  return static_cast<x11::Shape::So>(static_cast<T>(l) & static_cast<T>(r));
}

inline constexpr x11::Shape::Sk operator|(x11::Shape::Sk l, x11::Shape::Sk r) {
  using T = std::underlying_type_t<x11::Shape::Sk>;
  return static_cast<x11::Shape::Sk>(static_cast<T>(l) | static_cast<T>(r));
}

inline constexpr x11::Shape::Sk operator&(x11::Shape::Sk l, x11::Shape::Sk r) {
  using T = std::underlying_type_t<x11::Shape::Sk>;
  return static_cast<x11::Shape::Sk>(static_cast<T>(l) & static_cast<T>(r));
}

#endif  // UI_GFX_X_GENERATED_PROTOS_SHAPE_H_
