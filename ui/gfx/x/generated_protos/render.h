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

#ifndef UI_GFX_X_GENERATED_PROTOS_RENDER_H_
#define UI_GFX_X_GENERATED_PROTOS_RENDER_H_

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

class COMPONENT_EXPORT(X11) Render {
 public:
  static constexpr unsigned major_version = 0;
  static constexpr unsigned minor_version = 11;

  Render(Connection* connection, const x11::QueryExtensionReply& info);

  uint8_t present() const { return info_.present; }
  uint8_t major_opcode() const { return info_.major_opcode; }
  uint8_t first_event() const { return info_.first_event; }
  uint8_t first_error() const { return info_.first_error; }

  Connection* connection() const { return connection_; }

  enum class PictType : int {
    Indexed = 0,
    Direct = 1,
  };

  enum class Picture : uint32_t {
    None = 0,
  };

  enum class PictOp : int {
    Clear = 0,
    Src = 1,
    Dst = 2,
    Over = 3,
    OverReverse = 4,
    In = 5,
    InReverse = 6,
    Out = 7,
    OutReverse = 8,
    Atop = 9,
    AtopReverse = 10,
    Xor = 11,
    Add = 12,
    Saturate = 13,
    DisjointClear = 16,
    DisjointSrc = 17,
    DisjointDst = 18,
    DisjointOver = 19,
    DisjointOverReverse = 20,
    DisjointIn = 21,
    DisjointInReverse = 22,
    DisjointOut = 23,
    DisjointOutReverse = 24,
    DisjointAtop = 25,
    DisjointAtopReverse = 26,
    DisjointXor = 27,
    ConjointClear = 32,
    ConjointSrc = 33,
    ConjointDst = 34,
    ConjointOver = 35,
    ConjointOverReverse = 36,
    ConjointIn = 37,
    ConjointInReverse = 38,
    ConjointOut = 39,
    ConjointOutReverse = 40,
    ConjointAtop = 41,
    ConjointAtopReverse = 42,
    ConjointXor = 43,
    Multiply = 48,
    Screen = 49,
    Overlay = 50,
    Darken = 51,
    Lighten = 52,
    ColorDodge = 53,
    ColorBurn = 54,
    HardLight = 55,
    SoftLight = 56,
    Difference = 57,
    Exclusion = 58,
    HSLHue = 59,
    HSLSaturation = 60,
    HSLColor = 61,
    HSLLuminosity = 62,
  };

  enum class PolyEdge : int {
    Sharp = 0,
    Smooth = 1,
  };

  enum class PolyMode : int {
    Precise = 0,
    Imprecise = 1,
  };

  enum class CreatePictureAttribute : int {
    Repeat = 1 << 0,
    AlphaMap = 1 << 1,
    AlphaXOrigin = 1 << 2,
    AlphaYOrigin = 1 << 3,
    ClipXOrigin = 1 << 4,
    ClipYOrigin = 1 << 5,
    ClipMask = 1 << 6,
    GraphicsExposure = 1 << 7,
    SubwindowMode = 1 << 8,
    PolyEdge = 1 << 9,
    PolyMode = 1 << 10,
    Dither = 1 << 11,
    ComponentAlpha = 1 << 12,
  };

  enum class SubPixel : int {
    Unknown = 0,
    HorizontalRGB = 1,
    HorizontalBGR = 2,
    VerticalRGB = 3,
    VerticalBGR = 4,
    None = 5,
  };

  enum class Repeat : int {
    None = 0,
    Normal = 1,
    Pad = 2,
    Reflect = 3,
  };

  enum class Glyph : uint32_t {};

  enum class GlyphSet : uint32_t {};

  enum class PictFormat : uint32_t {};

  enum class Fixed : int32_t {};

  struct PictFormatError : public x11::Error {
    uint16_t sequence{};
    uint32_t bad_value{};
    uint16_t minor_opcode{};
    uint8_t major_opcode{};

    std::string ToString() const override;
  };

  struct PictureError : public x11::Error {
    uint16_t sequence{};
    uint32_t bad_value{};
    uint16_t minor_opcode{};
    uint8_t major_opcode{};

    std::string ToString() const override;
  };

  struct PictOpError : public x11::Error {
    uint16_t sequence{};
    uint32_t bad_value{};
    uint16_t minor_opcode{};
    uint8_t major_opcode{};

    std::string ToString() const override;
  };

  struct GlyphSetError : public x11::Error {
    uint16_t sequence{};
    uint32_t bad_value{};
    uint16_t minor_opcode{};
    uint8_t major_opcode{};

    std::string ToString() const override;
  };

  struct GlyphError : public x11::Error {
    uint16_t sequence{};
    uint32_t bad_value{};
    uint16_t minor_opcode{};
    uint8_t major_opcode{};

    std::string ToString() const override;
  };

  struct DirectFormat {
    bool operator==(const DirectFormat& other) const {
      return red_shift == other.red_shift && red_mask == other.red_mask &&
             green_shift == other.green_shift &&
             green_mask == other.green_mask && blue_shift == other.blue_shift &&
             blue_mask == other.blue_mask && alpha_shift == other.alpha_shift &&
             alpha_mask == other.alpha_mask;
    }

    uint16_t red_shift{};
    uint16_t red_mask{};
    uint16_t green_shift{};
    uint16_t green_mask{};
    uint16_t blue_shift{};
    uint16_t blue_mask{};
    uint16_t alpha_shift{};
    uint16_t alpha_mask{};
  };

  struct PictFormInfo {
    bool operator==(const PictFormInfo& other) const {
      return id == other.id && type == other.type && depth == other.depth &&
             direct == other.direct && colormap == other.colormap;
    }

    PictFormat id{};
    PictType type{};
    uint8_t depth{};
    DirectFormat direct{};
    ColorMap colormap{};
  };

  struct PictVisual {
    bool operator==(const PictVisual& other) const {
      return visual == other.visual && format == other.format;
    }

    VisualId visual{};
    PictFormat format{};
  };

  struct PictDepth {
    bool operator==(const PictDepth& other) const {
      return depth == other.depth && visuals == other.visuals;
    }

    uint8_t depth{};
    std::vector<PictVisual> visuals{};
  };

  struct PictScreen {
    bool operator==(const PictScreen& other) const {
      return fallback == other.fallback && depths == other.depths;
    }

    PictFormat fallback{};
    std::vector<PictDepth> depths{};
  };

  struct IndexValue {
    bool operator==(const IndexValue& other) const {
      return pixel == other.pixel && red == other.red && green == other.green &&
             blue == other.blue && alpha == other.alpha;
    }

    uint32_t pixel{};
    uint16_t red{};
    uint16_t green{};
    uint16_t blue{};
    uint16_t alpha{};
  };

  struct Color {
    bool operator==(const Color& other) const {
      return red == other.red && green == other.green && blue == other.blue &&
             alpha == other.alpha;
    }

    uint16_t red{};
    uint16_t green{};
    uint16_t blue{};
    uint16_t alpha{};
  };

  struct PointFix {
    bool operator==(const PointFix& other) const {
      return x == other.x && y == other.y;
    }

    Fixed x{};
    Fixed y{};
  };

  struct LineFix {
    bool operator==(const LineFix& other) const {
      return p1 == other.p1 && p2 == other.p2;
    }

    PointFix p1{};
    PointFix p2{};
  };

  struct Triangle {
    bool operator==(const Triangle& other) const {
      return p1 == other.p1 && p2 == other.p2 && p3 == other.p3;
    }

    PointFix p1{};
    PointFix p2{};
    PointFix p3{};
  };

  struct Trapezoid {
    bool operator==(const Trapezoid& other) const {
      return top == other.top && bottom == other.bottom && left == other.left &&
             right == other.right;
    }

    Fixed top{};
    Fixed bottom{};
    LineFix left{};
    LineFix right{};
  };

  struct GlyphInfo {
    bool operator==(const GlyphInfo& other) const {
      return width == other.width && height == other.height && x == other.x &&
             y == other.y && x_off == other.x_off && y_off == other.y_off;
    }

    uint16_t width{};
    uint16_t height{};
    int16_t x{};
    int16_t y{};
    int16_t x_off{};
    int16_t y_off{};
  };

  struct Transform {
    bool operator==(const Transform& other) const {
      return matrix11 == other.matrix11 && matrix12 == other.matrix12 &&
             matrix13 == other.matrix13 && matrix21 == other.matrix21 &&
             matrix22 == other.matrix22 && matrix23 == other.matrix23 &&
             matrix31 == other.matrix31 && matrix32 == other.matrix32 &&
             matrix33 == other.matrix33;
    }

    Fixed matrix11{};
    Fixed matrix12{};
    Fixed matrix13{};
    Fixed matrix21{};
    Fixed matrix22{};
    Fixed matrix23{};
    Fixed matrix31{};
    Fixed matrix32{};
    Fixed matrix33{};
  };

  struct AnimationCursorElement {
    bool operator==(const AnimationCursorElement& other) const {
      return cursor == other.cursor && delay == other.delay;
    }

    Cursor cursor{};
    uint32_t delay{};
  };

  struct SpanFix {
    bool operator==(const SpanFix& other) const {
      return l == other.l && r == other.r && y == other.y;
    }

    Fixed l{};
    Fixed r{};
    Fixed y{};
  };

  struct Trap {
    bool operator==(const Trap& other) const {
      return top == other.top && bot == other.bot;
    }

    SpanFix top{};
    SpanFix bot{};
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

  struct QueryPictFormatsRequest {};

  struct QueryPictFormatsReply {
    uint16_t sequence{};
    uint32_t num_depths{};
    uint32_t num_visuals{};
    std::vector<PictFormInfo> formats{};
    std::vector<PictScreen> screens{};
    std::vector<SubPixel> subpixels{};
  };

  using QueryPictFormatsResponse = Response<QueryPictFormatsReply>;

  Future<QueryPictFormatsReply> QueryPictFormats(
      const QueryPictFormatsRequest& request);

  Future<QueryPictFormatsReply> QueryPictFormats();

  struct QueryPictIndexValuesRequest {
    PictFormat format{};
  };

  struct QueryPictIndexValuesReply {
    uint16_t sequence{};
    std::vector<IndexValue> values{};
  };

  using QueryPictIndexValuesResponse = Response<QueryPictIndexValuesReply>;

  Future<QueryPictIndexValuesReply> QueryPictIndexValues(
      const QueryPictIndexValuesRequest& request);

  Future<QueryPictIndexValuesReply> QueryPictIndexValues(
      const PictFormat& format = {});

  struct CreatePictureRequest {
    Picture pid{};
    Drawable drawable{};
    PictFormat format{};
    std::optional<Repeat> repeat{};
    std::optional<Picture> alphamap{};
    std::optional<int32_t> alphaxorigin{};
    std::optional<int32_t> alphayorigin{};
    std::optional<int32_t> clipxorigin{};
    std::optional<int32_t> clipyorigin{};
    std::optional<Pixmap> clipmask{};
    std::optional<uint32_t> graphicsexposure{};
    std::optional<SubwindowMode> subwindowmode{};
    std::optional<PolyEdge> polyedge{};
    std::optional<PolyMode> polymode{};
    std::optional<Atom> dither{};
    std::optional<uint32_t> componentalpha{};
  };

  using CreatePictureResponse = Response<void>;

  Future<void> CreatePicture(const CreatePictureRequest& request);

  Future<void> CreatePicture(
      const Picture& pid = {},
      const Drawable& drawable = {},
      const PictFormat& format = {},
      const std::optional<Repeat>& repeat = std::nullopt,
      const std::optional<Picture>& alphamap = std::nullopt,
      const std::optional<int32_t>& alphaxorigin = std::nullopt,
      const std::optional<int32_t>& alphayorigin = std::nullopt,
      const std::optional<int32_t>& clipxorigin = std::nullopt,
      const std::optional<int32_t>& clipyorigin = std::nullopt,
      const std::optional<Pixmap>& clipmask = std::nullopt,
      const std::optional<uint32_t>& graphicsexposure = std::nullopt,
      const std::optional<SubwindowMode>& subwindowmode = std::nullopt,
      const std::optional<PolyEdge>& polyedge = std::nullopt,
      const std::optional<PolyMode>& polymode = std::nullopt,
      const std::optional<Atom>& dither = std::nullopt,
      const std::optional<uint32_t>& componentalpha = std::nullopt);

  struct ChangePictureRequest {
    Picture picture{};
    std::optional<Repeat> repeat{};
    std::optional<Picture> alphamap{};
    std::optional<int32_t> alphaxorigin{};
    std::optional<int32_t> alphayorigin{};
    std::optional<int32_t> clipxorigin{};
    std::optional<int32_t> clipyorigin{};
    std::optional<Pixmap> clipmask{};
    std::optional<uint32_t> graphicsexposure{};
    std::optional<SubwindowMode> subwindowmode{};
    std::optional<PolyEdge> polyedge{};
    std::optional<PolyMode> polymode{};
    std::optional<Atom> dither{};
    std::optional<uint32_t> componentalpha{};
  };

  using ChangePictureResponse = Response<void>;

  Future<void> ChangePicture(const ChangePictureRequest& request);

  Future<void> ChangePicture(
      const Picture& picture = {},
      const std::optional<Repeat>& repeat = std::nullopt,
      const std::optional<Picture>& alphamap = std::nullopt,
      const std::optional<int32_t>& alphaxorigin = std::nullopt,
      const std::optional<int32_t>& alphayorigin = std::nullopt,
      const std::optional<int32_t>& clipxorigin = std::nullopt,
      const std::optional<int32_t>& clipyorigin = std::nullopt,
      const std::optional<Pixmap>& clipmask = std::nullopt,
      const std::optional<uint32_t>& graphicsexposure = std::nullopt,
      const std::optional<SubwindowMode>& subwindowmode = std::nullopt,
      const std::optional<PolyEdge>& polyedge = std::nullopt,
      const std::optional<PolyMode>& polymode = std::nullopt,
      const std::optional<Atom>& dither = std::nullopt,
      const std::optional<uint32_t>& componentalpha = std::nullopt);

  struct SetPictureClipRectanglesRequest {
    Picture picture{};
    int16_t clip_x_origin{};
    int16_t clip_y_origin{};
    std::vector<Rectangle> rectangles{};
  };

  using SetPictureClipRectanglesResponse = Response<void>;

  Future<void> SetPictureClipRectangles(
      const SetPictureClipRectanglesRequest& request);

  Future<void> SetPictureClipRectangles(
      const Picture& picture = {},
      const int16_t& clip_x_origin = {},
      const int16_t& clip_y_origin = {},
      const std::vector<Rectangle>& rectangles = {});

  struct FreePictureRequest {
    Picture picture{};
  };

  using FreePictureResponse = Response<void>;

  Future<void> FreePicture(const FreePictureRequest& request);

  Future<void> FreePicture(const Picture& picture = {});

  struct CompositeRequest {
    PictOp op{};
    Picture src{};
    Picture mask{};
    Picture dst{};
    int16_t src_x{};
    int16_t src_y{};
    int16_t mask_x{};
    int16_t mask_y{};
    int16_t dst_x{};
    int16_t dst_y{};
    uint16_t width{};
    uint16_t height{};
  };

  using CompositeResponse = Response<void>;

  Future<void> Composite(const CompositeRequest& request);

  Future<void> Composite(const PictOp& op = {},
                         const Picture& src = {},
                         const Picture& mask = {},
                         const Picture& dst = {},
                         const int16_t& src_x = {},
                         const int16_t& src_y = {},
                         const int16_t& mask_x = {},
                         const int16_t& mask_y = {},
                         const int16_t& dst_x = {},
                         const int16_t& dst_y = {},
                         const uint16_t& width = {},
                         const uint16_t& height = {});

  struct TrapezoidsRequest {
    PictOp op{};
    Picture src{};
    Picture dst{};
    PictFormat mask_format{};
    int16_t src_x{};
    int16_t src_y{};
    std::vector<Trapezoid> traps{};
  };

  using TrapezoidsResponse = Response<void>;

  Future<void> Trapezoids(const TrapezoidsRequest& request);

  Future<void> Trapezoids(const PictOp& op = {},
                          const Picture& src = {},
                          const Picture& dst = {},
                          const PictFormat& mask_format = {},
                          const int16_t& src_x = {},
                          const int16_t& src_y = {},
                          const std::vector<Trapezoid>& traps = {});

  struct TrianglesRequest {
    PictOp op{};
    Picture src{};
    Picture dst{};
    PictFormat mask_format{};
    int16_t src_x{};
    int16_t src_y{};
    std::vector<Triangle> triangles{};
  };

  using TrianglesResponse = Response<void>;

  Future<void> Triangles(const TrianglesRequest& request);

  Future<void> Triangles(const PictOp& op = {},
                         const Picture& src = {},
                         const Picture& dst = {},
                         const PictFormat& mask_format = {},
                         const int16_t& src_x = {},
                         const int16_t& src_y = {},
                         const std::vector<Triangle>& triangles = {});

  struct TriStripRequest {
    PictOp op{};
    Picture src{};
    Picture dst{};
    PictFormat mask_format{};
    int16_t src_x{};
    int16_t src_y{};
    std::vector<PointFix> points{};
  };

  using TriStripResponse = Response<void>;

  Future<void> TriStrip(const TriStripRequest& request);

  Future<void> TriStrip(const PictOp& op = {},
                        const Picture& src = {},
                        const Picture& dst = {},
                        const PictFormat& mask_format = {},
                        const int16_t& src_x = {},
                        const int16_t& src_y = {},
                        const std::vector<PointFix>& points = {});

  struct TriFanRequest {
    PictOp op{};
    Picture src{};
    Picture dst{};
    PictFormat mask_format{};
    int16_t src_x{};
    int16_t src_y{};
    std::vector<PointFix> points{};
  };

  using TriFanResponse = Response<void>;

  Future<void> TriFan(const TriFanRequest& request);

  Future<void> TriFan(const PictOp& op = {},
                      const Picture& src = {},
                      const Picture& dst = {},
                      const PictFormat& mask_format = {},
                      const int16_t& src_x = {},
                      const int16_t& src_y = {},
                      const std::vector<PointFix>& points = {});

  struct CreateGlyphSetRequest {
    GlyphSet gsid{};
    PictFormat format{};
  };

  using CreateGlyphSetResponse = Response<void>;

  Future<void> CreateGlyphSet(const CreateGlyphSetRequest& request);

  Future<void> CreateGlyphSet(const GlyphSet& gsid = {},
                              const PictFormat& format = {});

  struct ReferenceGlyphSetRequest {
    GlyphSet gsid{};
    GlyphSet existing{};
  };

  using ReferenceGlyphSetResponse = Response<void>;

  Future<void> ReferenceGlyphSet(const ReferenceGlyphSetRequest& request);

  Future<void> ReferenceGlyphSet(const GlyphSet& gsid = {},
                                 const GlyphSet& existing = {});

  struct FreeGlyphSetRequest {
    GlyphSet glyphset{};
  };

  using FreeGlyphSetResponse = Response<void>;

  Future<void> FreeGlyphSet(const FreeGlyphSetRequest& request);

  Future<void> FreeGlyphSet(const GlyphSet& glyphset = {});

  struct AddGlyphsRequest {
    GlyphSet glyphset{};
    std::vector<uint32_t> glyphids{};
    std::vector<GlyphInfo> glyphs{};
    std::vector<uint8_t> data{};
  };

  using AddGlyphsResponse = Response<void>;

  Future<void> AddGlyphs(const AddGlyphsRequest& request);

  Future<void> AddGlyphs(const GlyphSet& glyphset = {},
                         const std::vector<uint32_t>& glyphids = {},
                         const std::vector<GlyphInfo>& glyphs = {},
                         const std::vector<uint8_t>& data = {});

  struct FreeGlyphsRequest {
    GlyphSet glyphset{};
    std::vector<Glyph> glyphs{};
  };

  using FreeGlyphsResponse = Response<void>;

  Future<void> FreeGlyphs(const FreeGlyphsRequest& request);

  Future<void> FreeGlyphs(const GlyphSet& glyphset = {},
                          const std::vector<Glyph>& glyphs = {});

  struct CompositeGlyphs8Request {
    PictOp op{};
    Picture src{};
    Picture dst{};
    PictFormat mask_format{};
    GlyphSet glyphset{};
    int16_t src_x{};
    int16_t src_y{};
    std::vector<uint8_t> glyphcmds{};
  };

  using CompositeGlyphs8Response = Response<void>;

  Future<void> CompositeGlyphs8(const CompositeGlyphs8Request& request);

  Future<void> CompositeGlyphs8(const PictOp& op = {},
                                const Picture& src = {},
                                const Picture& dst = {},
                                const PictFormat& mask_format = {},
                                const GlyphSet& glyphset = {},
                                const int16_t& src_x = {},
                                const int16_t& src_y = {},
                                const std::vector<uint8_t>& glyphcmds = {});

  struct CompositeGlyphs16Request {
    PictOp op{};
    Picture src{};
    Picture dst{};
    PictFormat mask_format{};
    GlyphSet glyphset{};
    int16_t src_x{};
    int16_t src_y{};
    std::vector<uint8_t> glyphcmds{};
  };

  using CompositeGlyphs16Response = Response<void>;

  Future<void> CompositeGlyphs16(const CompositeGlyphs16Request& request);

  Future<void> CompositeGlyphs16(const PictOp& op = {},
                                 const Picture& src = {},
                                 const Picture& dst = {},
                                 const PictFormat& mask_format = {},
                                 const GlyphSet& glyphset = {},
                                 const int16_t& src_x = {},
                                 const int16_t& src_y = {},
                                 const std::vector<uint8_t>& glyphcmds = {});

  struct CompositeGlyphs32Request {
    PictOp op{};
    Picture src{};
    Picture dst{};
    PictFormat mask_format{};
    GlyphSet glyphset{};
    int16_t src_x{};
    int16_t src_y{};
    std::vector<uint8_t> glyphcmds{};
  };

  using CompositeGlyphs32Response = Response<void>;

  Future<void> CompositeGlyphs32(const CompositeGlyphs32Request& request);

  Future<void> CompositeGlyphs32(const PictOp& op = {},
                                 const Picture& src = {},
                                 const Picture& dst = {},
                                 const PictFormat& mask_format = {},
                                 const GlyphSet& glyphset = {},
                                 const int16_t& src_x = {},
                                 const int16_t& src_y = {},
                                 const std::vector<uint8_t>& glyphcmds = {});

  struct FillRectanglesRequest {
    PictOp op{};
    Picture dst{};
    Color color{};
    std::vector<Rectangle> rects{};
  };

  using FillRectanglesResponse = Response<void>;

  Future<void> FillRectangles(const FillRectanglesRequest& request);

  Future<void> FillRectangles(const PictOp& op = {},
                              const Picture& dst = {},
                              const Color& color = {{}, {}, {}, {}},
                              const std::vector<Rectangle>& rects = {});

  struct CreateCursorRequest {
    Cursor cid{};
    Picture source{};
    uint16_t x{};
    uint16_t y{};
  };

  using CreateCursorResponse = Response<void>;

  Future<void> CreateCursor(const CreateCursorRequest& request);

  Future<void> CreateCursor(const Cursor& cid = {},
                            const Picture& source = {},
                            const uint16_t& x = {},
                            const uint16_t& y = {});

  struct SetPictureTransformRequest {
    Picture picture{};
    Transform transform{};
  };

  using SetPictureTransformResponse = Response<void>;

  Future<void> SetPictureTransform(const SetPictureTransformRequest& request);

  Future<void> SetPictureTransform(
      const Picture& picture = {},
      const Transform& transform = {{}, {}, {}, {}, {}, {}, {}, {}, {}});

  struct QueryFiltersRequest {
    Drawable drawable{};
  };

  struct QueryFiltersReply {
    uint16_t sequence{};
    std::vector<uint16_t> aliases{};
    std::vector<Str> filters{};
  };

  using QueryFiltersResponse = Response<QueryFiltersReply>;

  Future<QueryFiltersReply> QueryFilters(const QueryFiltersRequest& request);

  Future<QueryFiltersReply> QueryFilters(const Drawable& drawable = {});

  struct SetPictureFilterRequest {
    Picture picture{};
    std::string filter{};
    std::vector<Fixed> values{};
  };

  using SetPictureFilterResponse = Response<void>;

  Future<void> SetPictureFilter(const SetPictureFilterRequest& request);

  Future<void> SetPictureFilter(const Picture& picture = {},
                                const std::string& filter = {},
                                const std::vector<Fixed>& values = {});

  struct CreateAnimCursorRequest {
    Cursor cid{};
    std::vector<AnimationCursorElement> cursors{};
  };

  using CreateAnimCursorResponse = Response<void>;

  Future<void> CreateAnimCursor(const CreateAnimCursorRequest& request);

  Future<void> CreateAnimCursor(
      const Cursor& cid = {},
      const std::vector<AnimationCursorElement>& cursors = {});

  struct AddTrapsRequest {
    Picture picture{};
    int16_t x_off{};
    int16_t y_off{};
    std::vector<Trap> traps{};
  };

  using AddTrapsResponse = Response<void>;

  Future<void> AddTraps(const AddTrapsRequest& request);

  Future<void> AddTraps(const Picture& picture = {},
                        const int16_t& x_off = {},
                        const int16_t& y_off = {},
                        const std::vector<Trap>& traps = {});

  struct CreateSolidFillRequest {
    Picture picture{};
    Color color{};
  };

  using CreateSolidFillResponse = Response<void>;

  Future<void> CreateSolidFill(const CreateSolidFillRequest& request);

  Future<void> CreateSolidFill(const Picture& picture = {},
                               const Color& color = {{}, {}, {}, {}});

  struct CreateLinearGradientRequest {
    Picture picture{};
    PointFix p1{};
    PointFix p2{};
    std::vector<Fixed> stops{};
    std::vector<Color> colors{};
  };

  using CreateLinearGradientResponse = Response<void>;

  Future<void> CreateLinearGradient(const CreateLinearGradientRequest& request);

  Future<void> CreateLinearGradient(const Picture& picture = {},
                                    const PointFix& p1 = {{}, {}},
                                    const PointFix& p2 = {{}, {}},
                                    const std::vector<Fixed>& stops = {},
                                    const std::vector<Color>& colors = {});

  struct CreateRadialGradientRequest {
    Picture picture{};
    PointFix inner{};
    PointFix outer{};
    Fixed inner_radius{};
    Fixed outer_radius{};
    std::vector<Fixed> stops{};
    std::vector<Color> colors{};
  };

  using CreateRadialGradientResponse = Response<void>;

  Future<void> CreateRadialGradient(const CreateRadialGradientRequest& request);

  Future<void> CreateRadialGradient(const Picture& picture = {},
                                    const PointFix& inner = {{}, {}},
                                    const PointFix& outer = {{}, {}},
                                    const Fixed& inner_radius = {},
                                    const Fixed& outer_radius = {},
                                    const std::vector<Fixed>& stops = {},
                                    const std::vector<Color>& colors = {});

  struct CreateConicalGradientRequest {
    Picture picture{};
    PointFix center{};
    Fixed angle{};
    std::vector<Fixed> stops{};
    std::vector<Color> colors{};
  };

  using CreateConicalGradientResponse = Response<void>;

  Future<void> CreateConicalGradient(
      const CreateConicalGradientRequest& request);

  Future<void> CreateConicalGradient(const Picture& picture = {},
                                     const PointFix& center = {{}, {}},
                                     const Fixed& angle = {},
                                     const std::vector<Fixed>& stops = {},
                                     const std::vector<Color>& colors = {});

 private:
  Connection* const connection_;
  x11::QueryExtensionReply info_{};
};

}  // namespace x11

inline constexpr x11::Render::PictType operator|(x11::Render::PictType l,
                                                 x11::Render::PictType r) {
  using T = std::underlying_type_t<x11::Render::PictType>;
  return static_cast<x11::Render::PictType>(static_cast<T>(l) |
                                            static_cast<T>(r));
}

inline constexpr x11::Render::PictType operator&(x11::Render::PictType l,
                                                 x11::Render::PictType r) {
  using T = std::underlying_type_t<x11::Render::PictType>;
  return static_cast<x11::Render::PictType>(static_cast<T>(l) &
                                            static_cast<T>(r));
}

inline constexpr x11::Render::Picture operator|(x11::Render::Picture l,
                                                x11::Render::Picture r) {
  using T = std::underlying_type_t<x11::Render::Picture>;
  return static_cast<x11::Render::Picture>(static_cast<T>(l) |
                                           static_cast<T>(r));
}

inline constexpr x11::Render::Picture operator&(x11::Render::Picture l,
                                                x11::Render::Picture r) {
  using T = std::underlying_type_t<x11::Render::Picture>;
  return static_cast<x11::Render::Picture>(static_cast<T>(l) &
                                           static_cast<T>(r));
}

inline constexpr x11::Render::PictOp operator|(x11::Render::PictOp l,
                                               x11::Render::PictOp r) {
  using T = std::underlying_type_t<x11::Render::PictOp>;
  return static_cast<x11::Render::PictOp>(static_cast<T>(l) |
                                          static_cast<T>(r));
}

inline constexpr x11::Render::PictOp operator&(x11::Render::PictOp l,
                                               x11::Render::PictOp r) {
  using T = std::underlying_type_t<x11::Render::PictOp>;
  return static_cast<x11::Render::PictOp>(static_cast<T>(l) &
                                          static_cast<T>(r));
}

inline constexpr x11::Render::PolyEdge operator|(x11::Render::PolyEdge l,
                                                 x11::Render::PolyEdge r) {
  using T = std::underlying_type_t<x11::Render::PolyEdge>;
  return static_cast<x11::Render::PolyEdge>(static_cast<T>(l) |
                                            static_cast<T>(r));
}

inline constexpr x11::Render::PolyEdge operator&(x11::Render::PolyEdge l,
                                                 x11::Render::PolyEdge r) {
  using T = std::underlying_type_t<x11::Render::PolyEdge>;
  return static_cast<x11::Render::PolyEdge>(static_cast<T>(l) &
                                            static_cast<T>(r));
}

inline constexpr x11::Render::PolyMode operator|(x11::Render::PolyMode l,
                                                 x11::Render::PolyMode r) {
  using T = std::underlying_type_t<x11::Render::PolyMode>;
  return static_cast<x11::Render::PolyMode>(static_cast<T>(l) |
                                            static_cast<T>(r));
}

inline constexpr x11::Render::PolyMode operator&(x11::Render::PolyMode l,
                                                 x11::Render::PolyMode r) {
  using T = std::underlying_type_t<x11::Render::PolyMode>;
  return static_cast<x11::Render::PolyMode>(static_cast<T>(l) &
                                            static_cast<T>(r));
}

inline constexpr x11::Render::CreatePictureAttribute operator|(
    x11::Render::CreatePictureAttribute l,
    x11::Render::CreatePictureAttribute r) {
  using T = std::underlying_type_t<x11::Render::CreatePictureAttribute>;
  return static_cast<x11::Render::CreatePictureAttribute>(static_cast<T>(l) |
                                                          static_cast<T>(r));
}

inline constexpr x11::Render::CreatePictureAttribute operator&(
    x11::Render::CreatePictureAttribute l,
    x11::Render::CreatePictureAttribute r) {
  using T = std::underlying_type_t<x11::Render::CreatePictureAttribute>;
  return static_cast<x11::Render::CreatePictureAttribute>(static_cast<T>(l) &
                                                          static_cast<T>(r));
}

inline constexpr x11::Render::SubPixel operator|(x11::Render::SubPixel l,
                                                 x11::Render::SubPixel r) {
  using T = std::underlying_type_t<x11::Render::SubPixel>;
  return static_cast<x11::Render::SubPixel>(static_cast<T>(l) |
                                            static_cast<T>(r));
}

inline constexpr x11::Render::SubPixel operator&(x11::Render::SubPixel l,
                                                 x11::Render::SubPixel r) {
  using T = std::underlying_type_t<x11::Render::SubPixel>;
  return static_cast<x11::Render::SubPixel>(static_cast<T>(l) &
                                            static_cast<T>(r));
}

inline constexpr x11::Render::Repeat operator|(x11::Render::Repeat l,
                                               x11::Render::Repeat r) {
  using T = std::underlying_type_t<x11::Render::Repeat>;
  return static_cast<x11::Render::Repeat>(static_cast<T>(l) |
                                          static_cast<T>(r));
}

inline constexpr x11::Render::Repeat operator&(x11::Render::Repeat l,
                                               x11::Render::Repeat r) {
  using T = std::underlying_type_t<x11::Render::Repeat>;
  return static_cast<x11::Render::Repeat>(static_cast<T>(l) &
                                          static_cast<T>(r));
}

#endif  // UI_GFX_X_GENERATED_PROTOS_RENDER_H_
