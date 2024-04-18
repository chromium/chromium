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

#ifndef UI_GFX_X_GENERATED_PROTOS_XFIXES_H_
#define UI_GFX_X_GENERATED_PROTOS_XFIXES_H_

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <vector>

#include "base/component_export.h"
#include "base/files/scoped_file.h"
#include "base/memory/scoped_refptr.h"
#include "render.h"
#include "shape.h"
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

class COMPONENT_EXPORT(X11) XFixes {
 public:
  static constexpr unsigned major_version = 6;
  static constexpr unsigned minor_version = 0;

  XFixes(Connection* connection, const x11::QueryExtensionReply& info);

  uint8_t present() const { return info_.present; }
  uint8_t major_opcode() const { return info_.major_opcode; }
  uint8_t first_event() const { return info_.first_event; }
  uint8_t first_error() const { return info_.first_error; }

  Connection* connection() const { return connection_; }

  enum class SaveSetMode : int {
    Insert = 0,
    Delete = 1,
  };

  enum class SaveSetTarget : int {
    Nearest = 0,
    Root = 1,
  };

  enum class SaveSetMapping : int {
    Map = 0,
    Unmap = 1,
  };

  enum class SelectionEvent : int {
    SetSelectionOwner = 0,
    SelectionWindowDestroy = 1,
    SelectionClientClose = 2,
  };

  enum class SelectionEventMask : int {
    SetSelectionOwner = 1 << 0,
    SelectionWindowDestroy = 1 << 1,
    SelectionClientClose = 1 << 2,
  };

  enum class CursorNotify : int {
    DisplayCursor = 0,
  };

  enum class CursorNotifyMask : int {
    DisplayCursor = 1 << 0,
  };

  enum class Region : uint32_t {
    None = 0,
  };

  enum class Barrier : uint32_t {};

  enum class BarrierDirections : int {
    PositiveX = 1 << 0,
    PositiveY = 1 << 1,
    NegativeX = 1 << 2,
    NegativeY = 1 << 3,
  };

  enum class ClientDisconnectFlags : int {
    Default = 0,
    Terminate = 1 << 0,
  };

  struct SelectionNotifyEvent {
    static constexpr uint8_t type_id = 10;
    static constexpr uint8_t opcode = 0;
    SelectionEvent subtype{};
    uint16_t sequence{};
    Window window{};
    Window owner{};
    Atom selection{};
    Time timestamp{};
    Time selection_timestamp{};
  };

  struct CursorNotifyEvent {
    static constexpr uint8_t type_id = 11;
    static constexpr uint8_t opcode = 1;
    CursorNotify subtype{};
    uint16_t sequence{};
    Window window{};
    uint32_t cursor_serial{};
    Time timestamp{};
    Atom name{};
  };

  struct BadRegionError : public x11::Error {
    uint16_t sequence{};
    uint32_t bad_value{};
    uint16_t minor_opcode{};
    uint8_t major_opcode{};

    std::string ToString() const override;
  };

  struct QueryVersionRequest {
    uint32_t client_major_version{};
    uint32_t client_minor_version{};
  };

  struct QueryVersionReply {
    uint16_t sequence{};
    uint32_t major_version{};
    uint32_t minor_version{};
  };

  using QueryVersionResponse = Response<QueryVersionReply>;

  Future<QueryVersionReply> QueryVersion(const QueryVersionRequest& request);

  Future<QueryVersionReply> QueryVersion(
      const uint32_t& client_major_version = {},
      const uint32_t& client_minor_version = {});

  struct ChangeSaveSetRequest {
    SaveSetMode mode{};
    SaveSetTarget target{};
    SaveSetMapping map{};
    Window window{};
  };

  using ChangeSaveSetResponse = Response<void>;

  Future<void> ChangeSaveSet(const ChangeSaveSetRequest& request);

  Future<void> ChangeSaveSet(const SaveSetMode& mode = {},
                             const SaveSetTarget& target = {},
                             const SaveSetMapping& map = {},
                             const Window& window = {});

  struct SelectSelectionInputRequest {
    Window window{};
    Atom selection{};
    SelectionEventMask event_mask{};
  };

  using SelectSelectionInputResponse = Response<void>;

  Future<void> SelectSelectionInput(const SelectSelectionInputRequest& request);

  Future<void> SelectSelectionInput(const Window& window = {},
                                    const Atom& selection = {},
                                    const SelectionEventMask& event_mask = {});

  struct SelectCursorInputRequest {
    Window window{};
    CursorNotifyMask event_mask{};
  };

  using SelectCursorInputResponse = Response<void>;

  Future<void> SelectCursorInput(const SelectCursorInputRequest& request);

  Future<void> SelectCursorInput(const Window& window = {},
                                 const CursorNotifyMask& event_mask = {});

  struct GetCursorImageRequest {};

  struct GetCursorImageReply {
    uint16_t sequence{};
    int16_t x{};
    int16_t y{};
    uint16_t width{};
    uint16_t height{};
    uint16_t xhot{};
    uint16_t yhot{};
    uint32_t cursor_serial{};
    std::vector<uint32_t> cursor_image{};
  };

  using GetCursorImageResponse = Response<GetCursorImageReply>;

  Future<GetCursorImageReply> GetCursorImage(
      const GetCursorImageRequest& request);

  Future<GetCursorImageReply> GetCursorImage();

  struct CreateRegionRequest {
    Region region{};
    std::vector<Rectangle> rectangles{};
  };

  using CreateRegionResponse = Response<void>;

  Future<void> CreateRegion(const CreateRegionRequest& request);

  Future<void> CreateRegion(const Region& region = {},
                            const std::vector<Rectangle>& rectangles = {});

  struct CreateRegionFromBitmapRequest {
    Region region{};
    Pixmap bitmap{};
  };

  using CreateRegionFromBitmapResponse = Response<void>;

  Future<void> CreateRegionFromBitmap(
      const CreateRegionFromBitmapRequest& request);

  Future<void> CreateRegionFromBitmap(const Region& region = {},
                                      const Pixmap& bitmap = {});

  struct CreateRegionFromWindowRequest {
    Region region{};
    Window window{};
    Shape::Sk kind{};
  };

  using CreateRegionFromWindowResponse = Response<void>;

  Future<void> CreateRegionFromWindow(
      const CreateRegionFromWindowRequest& request);

  Future<void> CreateRegionFromWindow(const Region& region = {},
                                      const Window& window = {},
                                      const Shape::Sk& kind = {});

  struct CreateRegionFromGCRequest {
    Region region{};
    GraphicsContext gc{};
  };

  using CreateRegionFromGCResponse = Response<void>;

  Future<void> CreateRegionFromGC(const CreateRegionFromGCRequest& request);

  Future<void> CreateRegionFromGC(const Region& region = {},
                                  const GraphicsContext& gc = {});

  struct CreateRegionFromPictureRequest {
    Region region{};
    Render::Picture picture{};
  };

  using CreateRegionFromPictureResponse = Response<void>;

  Future<void> CreateRegionFromPicture(
      const CreateRegionFromPictureRequest& request);

  Future<void> CreateRegionFromPicture(const Region& region = {},
                                       const Render::Picture& picture = {});

  struct DestroyRegionRequest {
    Region region{};
  };

  using DestroyRegionResponse = Response<void>;

  Future<void> DestroyRegion(const DestroyRegionRequest& request);

  Future<void> DestroyRegion(const Region& region = {});

  struct SetRegionRequest {
    Region region{};
    std::vector<Rectangle> rectangles{};
  };

  using SetRegionResponse = Response<void>;

  Future<void> SetRegion(const SetRegionRequest& request);

  Future<void> SetRegion(const Region& region = {},
                         const std::vector<Rectangle>& rectangles = {});

  struct CopyRegionRequest {
    Region source{};
    Region destination{};
  };

  using CopyRegionResponse = Response<void>;

  Future<void> CopyRegion(const CopyRegionRequest& request);

  Future<void> CopyRegion(const Region& source = {},
                          const Region& destination = {});

  struct UnionRegionRequest {
    Region source1{};
    Region source2{};
    Region destination{};
  };

  using UnionRegionResponse = Response<void>;

  Future<void> UnionRegion(const UnionRegionRequest& request);

  Future<void> UnionRegion(const Region& source1 = {},
                           const Region& source2 = {},
                           const Region& destination = {});

  struct IntersectRegionRequest {
    Region source1{};
    Region source2{};
    Region destination{};
  };

  using IntersectRegionResponse = Response<void>;

  Future<void> IntersectRegion(const IntersectRegionRequest& request);

  Future<void> IntersectRegion(const Region& source1 = {},
                               const Region& source2 = {},
                               const Region& destination = {});

  struct SubtractRegionRequest {
    Region source1{};
    Region source2{};
    Region destination{};
  };

  using SubtractRegionResponse = Response<void>;

  Future<void> SubtractRegion(const SubtractRegionRequest& request);

  Future<void> SubtractRegion(const Region& source1 = {},
                              const Region& source2 = {},
                              const Region& destination = {});

  struct InvertRegionRequest {
    Region source{};
    Rectangle bounds{};
    Region destination{};
  };

  using InvertRegionResponse = Response<void>;

  Future<void> InvertRegion(const InvertRegionRequest& request);

  Future<void> InvertRegion(const Region& source = {},
                            const Rectangle& bounds = {{}, {}, {}, {}},
                            const Region& destination = {});

  struct TranslateRegionRequest {
    Region region{};
    int16_t dx{};
    int16_t dy{};
  };

  using TranslateRegionResponse = Response<void>;

  Future<void> TranslateRegion(const TranslateRegionRequest& request);

  Future<void> TranslateRegion(const Region& region = {},
                               const int16_t& dx = {},
                               const int16_t& dy = {});

  struct RegionExtentsRequest {
    Region source{};
    Region destination{};
  };

  using RegionExtentsResponse = Response<void>;

  Future<void> RegionExtents(const RegionExtentsRequest& request);

  Future<void> RegionExtents(const Region& source = {},
                             const Region& destination = {});

  struct FetchRegionRequest {
    Region region{};
  };

  struct FetchRegionReply {
    uint16_t sequence{};
    Rectangle extents{};
    std::vector<Rectangle> rectangles{};
  };

  using FetchRegionResponse = Response<FetchRegionReply>;

  Future<FetchRegionReply> FetchRegion(const FetchRegionRequest& request);

  Future<FetchRegionReply> FetchRegion(const Region& region = {});

  struct SetGCClipRegionRequest {
    GraphicsContext gc{};
    Region region{};
    int16_t x_origin{};
    int16_t y_origin{};
  };

  using SetGCClipRegionResponse = Response<void>;

  Future<void> SetGCClipRegion(const SetGCClipRegionRequest& request);

  Future<void> SetGCClipRegion(const GraphicsContext& gc = {},
                               const Region& region = {},
                               const int16_t& x_origin = {},
                               const int16_t& y_origin = {});

  struct SetWindowShapeRegionRequest {
    Window dest{};
    Shape::Sk dest_kind{};
    int16_t x_offset{};
    int16_t y_offset{};
    Region region{};
  };

  using SetWindowShapeRegionResponse = Response<void>;

  Future<void> SetWindowShapeRegion(const SetWindowShapeRegionRequest& request);

  Future<void> SetWindowShapeRegion(const Window& dest = {},
                                    const Shape::Sk& dest_kind = {},
                                    const int16_t& x_offset = {},
                                    const int16_t& y_offset = {},
                                    const Region& region = {});

  struct SetPictureClipRegionRequest {
    Render::Picture picture{};
    Region region{};
    int16_t x_origin{};
    int16_t y_origin{};
  };

  using SetPictureClipRegionResponse = Response<void>;

  Future<void> SetPictureClipRegion(const SetPictureClipRegionRequest& request);

  Future<void> SetPictureClipRegion(const Render::Picture& picture = {},
                                    const Region& region = {},
                                    const int16_t& x_origin = {},
                                    const int16_t& y_origin = {});

  struct SetCursorNameRequest {
    Cursor cursor{};
    std::string name{};
  };

  using SetCursorNameResponse = Response<void>;

  Future<void> SetCursorName(const SetCursorNameRequest& request);

  Future<void> SetCursorName(const Cursor& cursor = {},
                             const std::string& name = {});

  struct GetCursorNameRequest {
    Cursor cursor{};
  };

  struct GetCursorNameReply {
    uint16_t sequence{};
    Atom atom{};
    std::string name{};
  };

  using GetCursorNameResponse = Response<GetCursorNameReply>;

  Future<GetCursorNameReply> GetCursorName(const GetCursorNameRequest& request);

  Future<GetCursorNameReply> GetCursorName(const Cursor& cursor = {});

  struct GetCursorImageAndNameRequest {};

  struct GetCursorImageAndNameReply {
    uint16_t sequence{};
    int16_t x{};
    int16_t y{};
    uint16_t width{};
    uint16_t height{};
    uint16_t xhot{};
    uint16_t yhot{};
    uint32_t cursor_serial{};
    Atom cursor_atom{};
    std::vector<uint32_t> cursor_image{};
    std::string name{};
  };

  using GetCursorImageAndNameResponse = Response<GetCursorImageAndNameReply>;

  Future<GetCursorImageAndNameReply> GetCursorImageAndName(
      const GetCursorImageAndNameRequest& request);

  Future<GetCursorImageAndNameReply> GetCursorImageAndName();

  struct ChangeCursorRequest {
    Cursor source{};
    Cursor destination{};
  };

  using ChangeCursorResponse = Response<void>;

  Future<void> ChangeCursor(const ChangeCursorRequest& request);

  Future<void> ChangeCursor(const Cursor& source = {},
                            const Cursor& destination = {});

  struct ChangeCursorByNameRequest {
    Cursor src{};
    std::string name{};
  };

  using ChangeCursorByNameResponse = Response<void>;

  Future<void> ChangeCursorByName(const ChangeCursorByNameRequest& request);

  Future<void> ChangeCursorByName(const Cursor& src = {},
                                  const std::string& name = {});

  struct ExpandRegionRequest {
    Region source{};
    Region destination{};
    uint16_t left{};
    uint16_t right{};
    uint16_t top{};
    uint16_t bottom{};
  };

  using ExpandRegionResponse = Response<void>;

  Future<void> ExpandRegion(const ExpandRegionRequest& request);

  Future<void> ExpandRegion(const Region& source = {},
                            const Region& destination = {},
                            const uint16_t& left = {},
                            const uint16_t& right = {},
                            const uint16_t& top = {},
                            const uint16_t& bottom = {});

  struct HideCursorRequest {
    Window window{};
  };

  using HideCursorResponse = Response<void>;

  Future<void> HideCursor(const HideCursorRequest& request);

  Future<void> HideCursor(const Window& window = {});

  struct ShowCursorRequest {
    Window window{};
  };

  using ShowCursorResponse = Response<void>;

  Future<void> ShowCursor(const ShowCursorRequest& request);

  Future<void> ShowCursor(const Window& window = {});

  struct CreatePointerBarrierRequest {
    Barrier barrier{};
    Window window{};
    uint16_t x1{};
    uint16_t y1{};
    uint16_t x2{};
    uint16_t y2{};
    BarrierDirections directions{};
    std::vector<uint16_t> devices{};
  };

  using CreatePointerBarrierResponse = Response<void>;

  Future<void> CreatePointerBarrier(const CreatePointerBarrierRequest& request);

  Future<void> CreatePointerBarrier(const Barrier& barrier = {},
                                    const Window& window = {},
                                    const uint16_t& x1 = {},
                                    const uint16_t& y1 = {},
                                    const uint16_t& x2 = {},
                                    const uint16_t& y2 = {},
                                    const BarrierDirections& directions = {},
                                    const std::vector<uint16_t>& devices = {});

  struct DeletePointerBarrierRequest {
    Barrier barrier{};
  };

  using DeletePointerBarrierResponse = Response<void>;

  Future<void> DeletePointerBarrier(const DeletePointerBarrierRequest& request);

  Future<void> DeletePointerBarrier(const Barrier& barrier = {});

  struct SetClientDisconnectModeRequest {
    ClientDisconnectFlags disconnect_mode{};
  };

  using SetClientDisconnectModeResponse = Response<void>;

  Future<void> SetClientDisconnectMode(
      const SetClientDisconnectModeRequest& request);

  Future<void> SetClientDisconnectMode(
      const ClientDisconnectFlags& disconnect_mode = {});

  struct GetClientDisconnectModeRequest {};

  struct GetClientDisconnectModeReply {
    uint16_t sequence{};
    ClientDisconnectFlags disconnect_mode{};
  };

  using GetClientDisconnectModeResponse =
      Response<GetClientDisconnectModeReply>;

  Future<GetClientDisconnectModeReply> GetClientDisconnectMode(
      const GetClientDisconnectModeRequest& request);

  Future<GetClientDisconnectModeReply> GetClientDisconnectMode();

 private:
  Connection* const connection_;
  x11::QueryExtensionReply info_{};
};

}  // namespace x11

inline constexpr x11::XFixes::SaveSetMode operator|(
    x11::XFixes::SaveSetMode l,
    x11::XFixes::SaveSetMode r) {
  using T = std::underlying_type_t<x11::XFixes::SaveSetMode>;
  return static_cast<x11::XFixes::SaveSetMode>(static_cast<T>(l) |
                                               static_cast<T>(r));
}

inline constexpr x11::XFixes::SaveSetMode operator&(
    x11::XFixes::SaveSetMode l,
    x11::XFixes::SaveSetMode r) {
  using T = std::underlying_type_t<x11::XFixes::SaveSetMode>;
  return static_cast<x11::XFixes::SaveSetMode>(static_cast<T>(l) &
                                               static_cast<T>(r));
}

inline constexpr x11::XFixes::SaveSetTarget operator|(
    x11::XFixes::SaveSetTarget l,
    x11::XFixes::SaveSetTarget r) {
  using T = std::underlying_type_t<x11::XFixes::SaveSetTarget>;
  return static_cast<x11::XFixes::SaveSetTarget>(static_cast<T>(l) |
                                                 static_cast<T>(r));
}

inline constexpr x11::XFixes::SaveSetTarget operator&(
    x11::XFixes::SaveSetTarget l,
    x11::XFixes::SaveSetTarget r) {
  using T = std::underlying_type_t<x11::XFixes::SaveSetTarget>;
  return static_cast<x11::XFixes::SaveSetTarget>(static_cast<T>(l) &
                                                 static_cast<T>(r));
}

inline constexpr x11::XFixes::SaveSetMapping operator|(
    x11::XFixes::SaveSetMapping l,
    x11::XFixes::SaveSetMapping r) {
  using T = std::underlying_type_t<x11::XFixes::SaveSetMapping>;
  return static_cast<x11::XFixes::SaveSetMapping>(static_cast<T>(l) |
                                                  static_cast<T>(r));
}

inline constexpr x11::XFixes::SaveSetMapping operator&(
    x11::XFixes::SaveSetMapping l,
    x11::XFixes::SaveSetMapping r) {
  using T = std::underlying_type_t<x11::XFixes::SaveSetMapping>;
  return static_cast<x11::XFixes::SaveSetMapping>(static_cast<T>(l) &
                                                  static_cast<T>(r));
}

inline constexpr x11::XFixes::SelectionEvent operator|(
    x11::XFixes::SelectionEvent l,
    x11::XFixes::SelectionEvent r) {
  using T = std::underlying_type_t<x11::XFixes::SelectionEvent>;
  return static_cast<x11::XFixes::SelectionEvent>(static_cast<T>(l) |
                                                  static_cast<T>(r));
}

inline constexpr x11::XFixes::SelectionEvent operator&(
    x11::XFixes::SelectionEvent l,
    x11::XFixes::SelectionEvent r) {
  using T = std::underlying_type_t<x11::XFixes::SelectionEvent>;
  return static_cast<x11::XFixes::SelectionEvent>(static_cast<T>(l) &
                                                  static_cast<T>(r));
}

inline constexpr x11::XFixes::SelectionEventMask operator|(
    x11::XFixes::SelectionEventMask l,
    x11::XFixes::SelectionEventMask r) {
  using T = std::underlying_type_t<x11::XFixes::SelectionEventMask>;
  return static_cast<x11::XFixes::SelectionEventMask>(static_cast<T>(l) |
                                                      static_cast<T>(r));
}

inline constexpr x11::XFixes::SelectionEventMask operator&(
    x11::XFixes::SelectionEventMask l,
    x11::XFixes::SelectionEventMask r) {
  using T = std::underlying_type_t<x11::XFixes::SelectionEventMask>;
  return static_cast<x11::XFixes::SelectionEventMask>(static_cast<T>(l) &
                                                      static_cast<T>(r));
}

inline constexpr x11::XFixes::CursorNotify operator|(
    x11::XFixes::CursorNotify l,
    x11::XFixes::CursorNotify r) {
  using T = std::underlying_type_t<x11::XFixes::CursorNotify>;
  return static_cast<x11::XFixes::CursorNotify>(static_cast<T>(l) |
                                                static_cast<T>(r));
}

inline constexpr x11::XFixes::CursorNotify operator&(
    x11::XFixes::CursorNotify l,
    x11::XFixes::CursorNotify r) {
  using T = std::underlying_type_t<x11::XFixes::CursorNotify>;
  return static_cast<x11::XFixes::CursorNotify>(static_cast<T>(l) &
                                                static_cast<T>(r));
}

inline constexpr x11::XFixes::CursorNotifyMask operator|(
    x11::XFixes::CursorNotifyMask l,
    x11::XFixes::CursorNotifyMask r) {
  using T = std::underlying_type_t<x11::XFixes::CursorNotifyMask>;
  return static_cast<x11::XFixes::CursorNotifyMask>(static_cast<T>(l) |
                                                    static_cast<T>(r));
}

inline constexpr x11::XFixes::CursorNotifyMask operator&(
    x11::XFixes::CursorNotifyMask l,
    x11::XFixes::CursorNotifyMask r) {
  using T = std::underlying_type_t<x11::XFixes::CursorNotifyMask>;
  return static_cast<x11::XFixes::CursorNotifyMask>(static_cast<T>(l) &
                                                    static_cast<T>(r));
}

inline constexpr x11::XFixes::Region operator|(x11::XFixes::Region l,
                                               x11::XFixes::Region r) {
  using T = std::underlying_type_t<x11::XFixes::Region>;
  return static_cast<x11::XFixes::Region>(static_cast<T>(l) |
                                          static_cast<T>(r));
}

inline constexpr x11::XFixes::Region operator&(x11::XFixes::Region l,
                                               x11::XFixes::Region r) {
  using T = std::underlying_type_t<x11::XFixes::Region>;
  return static_cast<x11::XFixes::Region>(static_cast<T>(l) &
                                          static_cast<T>(r));
}

inline constexpr x11::XFixes::BarrierDirections operator|(
    x11::XFixes::BarrierDirections l,
    x11::XFixes::BarrierDirections r) {
  using T = std::underlying_type_t<x11::XFixes::BarrierDirections>;
  return static_cast<x11::XFixes::BarrierDirections>(static_cast<T>(l) |
                                                     static_cast<T>(r));
}

inline constexpr x11::XFixes::BarrierDirections operator&(
    x11::XFixes::BarrierDirections l,
    x11::XFixes::BarrierDirections r) {
  using T = std::underlying_type_t<x11::XFixes::BarrierDirections>;
  return static_cast<x11::XFixes::BarrierDirections>(static_cast<T>(l) &
                                                     static_cast<T>(r));
}

inline constexpr x11::XFixes::ClientDisconnectFlags operator|(
    x11::XFixes::ClientDisconnectFlags l,
    x11::XFixes::ClientDisconnectFlags r) {
  using T = std::underlying_type_t<x11::XFixes::ClientDisconnectFlags>;
  return static_cast<x11::XFixes::ClientDisconnectFlags>(static_cast<T>(l) |
                                                         static_cast<T>(r));
}

inline constexpr x11::XFixes::ClientDisconnectFlags operator&(
    x11::XFixes::ClientDisconnectFlags l,
    x11::XFixes::ClientDisconnectFlags r) {
  using T = std::underlying_type_t<x11::XFixes::ClientDisconnectFlags>;
  return static_cast<x11::XFixes::ClientDisconnectFlags>(static_cast<T>(l) &
                                                         static_cast<T>(r));
}

#endif  // UI_GFX_X_GENERATED_PROTOS_XFIXES_H_
