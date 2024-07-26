// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/wm/core/cursor_util.h"

#include <cfloat>
#include <memory>
#include <optional>

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "cc/paint/skottie_wrapper.h"
#include "skia/ext/image_operations.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/cursor_size.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/resource/resource_scale_factor.h"
#include "ui/display/display_transform.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "ui/gfx/skbitmap_operations.h"
#include "ui/lottie/animation.h"
#include "ui/resources/grit/ui_resources.h"

namespace wm {

namespace {

using ::ui::mojom::CursorType;

using AnimationCache =
    base::flat_map<CursorType, std::unique_ptr<lottie::Animation>>;

// Get a cache for lottie animations.
AnimationCache& GetAnimationCache() {
  static base::NoDestructor<AnimationCache> cache;
  return *cache;
}

// Converts the SkBitmap to use a different alpha type. Returns true if bitmap
// was modified, otherwise returns false.
bool ConvertSkBitmapAlphaType(SkBitmap* bitmap, SkAlphaType alpha_type) {
  if (bitmap->info().alphaType() == alpha_type) {
    return false;
  }

  // Copy the bitmap into a temporary buffer. This will convert alpha type.
  SkImageInfo image_info =
      SkImageInfo::MakeN32(bitmap->width(), bitmap->height(), alpha_type);
  size_t info_row_bytes = image_info.minRowBytes();
  std::vector<char> buffer(image_info.computeByteSize(info_row_bytes));
  bitmap->readPixels(image_info, &buffer[0], info_row_bytes, 0, 0);
  // Read the temporary buffer back into the original bitmap.
  bitmap->reset();
  bitmap->allocPixels(image_info);
  // this memcpy call assumes bitmap->rowBytes() == info_row_bytes
  memcpy(bitmap->getPixels(), &buffer[0], buffer.size());

  return true;
}

// Only rotate the cursor's hotpoint. |hotpoint_in_out| is used as
// both input and output. |cursor_bitmap_width| and |cursor_bitmap_height|
// should be the width and height of the cursor before bitmap rotation.
void RotateCursorHotpoint(display::Display::Rotation rotation,
                          int cursor_bitmap_width,
                          int cursor_bitmap_height,
                          gfx::Point* hotpoint_in_out) {
  switch (rotation) {
    case display::Display::ROTATE_0:
      break;
    case display::Display::ROTATE_90:
      hotpoint_in_out->SetPoint(cursor_bitmap_height - hotpoint_in_out->y(),
                                hotpoint_in_out->x());
      break;
    case display::Display::ROTATE_180:
      hotpoint_in_out->SetPoint(cursor_bitmap_width - hotpoint_in_out->x(),
                                cursor_bitmap_height - hotpoint_in_out->y());
      break;
    case display::Display::ROTATE_270:
      hotpoint_in_out->SetPoint(hotpoint_in_out->y(),
                                cursor_bitmap_width - hotpoint_in_out->x());
      break;
  }
}

// Rotate the cursor's bitmap and hotpoint.
// |bitmap_in_out| and |hotpoint_in_out| are used as
// both input and output.
void RotateCursorBitmapAndHotpoint(display::Display::Rotation rotation,
                                   SkBitmap* bitmap_in_out,
                                   gfx::Point* hotpoint_in_out) {
  if (hotpoint_in_out) {
    RotateCursorHotpoint(rotation, bitmap_in_out->width(),
                         bitmap_in_out->height(), hotpoint_in_out);
  }

  // SkBitmapOperations::Rotate() needs the bitmap to have premultiplied alpha.
  DCHECK(rotation == display::Display::ROTATE_0 ||
         bitmap_in_out->info().alphaType() != kUnpremul_SkAlphaType);

  switch (rotation) {
    case display::Display::ROTATE_0:
      break;
    case display::Display::ROTATE_90:
      *bitmap_in_out = SkBitmapOperations::Rotate(
          *bitmap_in_out, SkBitmapOperations::ROTATION_90_CW);
      break;
    case display::Display::ROTATE_180:
      *bitmap_in_out = SkBitmapOperations::Rotate(
          *bitmap_in_out, SkBitmapOperations::ROTATION_180_CW);
      break;
    case display::Display::ROTATE_270:
      *bitmap_in_out = SkBitmapOperations::Rotate(
          *bitmap_in_out, SkBitmapOperations::ROTATION_270_CW);
      break;
  }
}

// Create bitmaps from bitmap pixels. |image_rep| has a scale and holds
// bitmap pixels for that scale.
void CreateBitmapsFromPixels(const gfx::ImageSkiaRep& image_rep,
                             float scale,
                             display::Display::Rotation rotation,
                             bool is_animated,
                             std::vector<SkBitmap>* bitmaps_out,
                             gfx::Point* hotspot_in_out) {
  CHECK(bitmaps_out->empty());

  SkBitmap bitmap = image_rep.GetBitmap();
  if (!is_animated) {
    // Non-animated cursor.
    ScaleAndRotateCursorBitmapAndHotpoint(scale / image_rep.scale(), rotation,
                                          &bitmap, hotspot_in_out);

    bitmaps_out->push_back(std::move(bitmap));
  } else {
    // Animated cursor.

    // The image is assumed to be a concatenation of animation frames from
    // left to right. Also, each frame is assumed to be square (width ==
    // height).
    const int frame_width = bitmap.height();
    const int frame_height = frame_width;
    const int total_width = bitmap.width();
    CHECK_EQ(total_width % frame_width, 0);
    const int frame_count = total_width / frame_width;
    CHECK_GT(frame_count, 0);

    for (int frame = 0; frame < frame_count; ++frame) {
      const int x_offset = frame_width * frame;
      SkBitmap cropped = SkBitmapOperations::CreateTiledBitmap(
          bitmap, x_offset, 0, frame_width, frame_height);
      ScaleAndRotateCursorBitmapAndHotpoint(
          scale / image_rep.scale(), rotation, &cropped,
          frame == 0 ? hotspot_in_out : nullptr);

      bitmaps_out->push_back(std::move(cropped));
    }
  }
}

// Create bitmaps from static lottie. |image_rep| is unscaled and contains
// a paint record for the lottie animation.
void CreateBitmapsFromStaticLottie(const gfx::ImageSkiaRep& image_rep,
                                   const gfx::Size& scaled_size,
                                   float scale,
                                   const gfx::Transform& rotation_transform,
                                   std::vector<SkBitmap>* bitmaps_out) {
  CHECK(bitmaps_out->empty());

  // Non-animated cursor.
  SkBitmap bitmap;
  bitmap.allocN32Pixels(scaled_size.width(), scaled_size.height());
  bitmap.eraseColor(SK_ColorTRANSPARENT);

  SkCanvas canvas(bitmap);
  canvas.concat(TransformToSkM44(rotation_transform));
  canvas.scale(scale, scale);
  image_rep.GetPaintRecord().Playback(&canvas);
  bitmap.setImmutable();

  bitmaps_out->push_back(std::move(bitmap));
}

// Create bitmaps from animated lottie.
void CreateBitmapsFromAnimatedLottie(int resource_id,
                                     const gfx::Size& scaled_size,
                                     float scale,
                                     const gfx::Transform& rotation_transform,
                                     CursorType type,
                                     std::vector<SkBitmap>* bitmaps_out) {
  CHECK(bitmaps_out->empty());

  AnimationCache& cursor_animations = GetAnimationCache();
  if (!base::Contains(cursor_animations, type)) {
    std::optional<std::vector<uint8_t>> lottie_bytes =
        ui::ResourceBundle::GetSharedInstance().GetLottieData(resource_id);
    scoped_refptr<cc::SkottieWrapper> skottie =
        cc::SkottieWrapper::UnsafeCreateSerializable(std::move(*lottie_bytes));
    cursor_animations[type] = std::make_unique<lottie::Animation>(skottie);
  }
  lottie::Animation* animation = cursor_animations[type].get();
  const float cursor_animation_duration_in_second =
      animation->GetAnimationDuration().InSecondsF();

  // Target frame rate for animated cursor.
  const int kAnimatedCursorFramePerSecond = 60;
  const int frames =
      kAnimatedCursorFramePerSecond * cursor_animation_duration_in_second;

  for (int i = 0; i < frames; i++) {
    float t = static_cast<float>(i) / frames;

    SkBitmap bitmap;
    bitmap.allocN32Pixels(scaled_size.width(), scaled_size.height());
    bitmap.eraseColor(SK_ColorTRANSPARENT);

    cc::SkiaPaintCanvas paint_canvas(bitmap);
    gfx::Canvas canvas(&paint_canvas, scale);
    canvas.Transform(rotation_transform);
    animation->PaintFrame(&canvas, t, scaled_size);
    bitmap.setImmutable();

    bitmaps_out->push_back(std::move(bitmap));
  }
}

struct CursorResourceData {
  CursorType type;
  int id;
  gfx::Point hotspot_1x;
  gfx::Point hotspot_2x;
  bool is_animated = false;
};

// Cursor resource data indexed by CursorType. Make sure to respect the order
// defined at ui/base/cursor/mojom/cursor_type.mojom.
constexpr std::optional<CursorResourceData> kNormalCursorResourceData[] = {
    {{CursorType::kPointer, IDR_AURA_CURSOR_PTR, {4, 4}, {7, 7}}},
    {{CursorType::kCross, IDR_AURA_CURSOR_CROSSHAIR, {12, 12}, {24, 24}}},
    {{CursorType::kHand, IDR_AURA_CURSOR_HAND, {9, 4}, {19, 8}}},
    {{CursorType::kIBeam, IDR_AURA_CURSOR_IBEAM, {12, 12}, {24, 25}}},
    {{CursorType::kWait,
      IDR_AURA_CURSOR_THROBBER,
      {7, 7},
      {14, 14},
      /*is_animated=*/true}},
    {{CursorType::kHelp, IDR_AURA_CURSOR_HELP, {4, 4}, {8, 9}}},
    {{CursorType::kEastResize,
      IDR_AURA_CURSOR_EAST_RESIZE,
      {12, 11},
      {25, 23}}},
    {{CursorType::kNorthResize,
      IDR_AURA_CURSOR_NORTH_RESIZE,
      {11, 12},
      {23, 23}}},
    {{CursorType::kNorthEastResize,
      IDR_AURA_CURSOR_NORTH_EAST_RESIZE,
      {12, 11},
      {25, 23}}},
    {{CursorType::kNorthWestResize,
      IDR_AURA_CURSOR_NORTH_WEST_RESIZE,
      {11, 11},
      {24, 23}}},
    {{CursorType::kSouthResize,
      IDR_AURA_CURSOR_SOUTH_RESIZE,
      {11, 12},
      {23, 23}}},
    {{CursorType::kSouthEastResize,
      IDR_AURA_CURSOR_SOUTH_EAST_RESIZE,
      {11, 11},
      {24, 23}}},
    {{CursorType::kSouthWestResize,
      IDR_AURA_CURSOR_SOUTH_WEST_RESIZE,
      {12, 11},
      {25, 23}}},
    {{CursorType::kWestResize,
      IDR_AURA_CURSOR_WEST_RESIZE,
      {12, 11},
      {25, 23}}},
    {{CursorType::kNorthSouthResize,
      IDR_AURA_CURSOR_NORTH_SOUTH_RESIZE,
      {11, 12},
      {23, 23}}},
    {{CursorType::kEastWestResize,
      IDR_AURA_CURSOR_EAST_WEST_RESIZE,
      {12, 11},
      {25, 23}}},
    {{CursorType::kNorthEastSouthWestResize,
      IDR_AURA_CURSOR_NORTH_EAST_SOUTH_WEST_RESIZE,
      {12, 11},
      {25, 23}}},
    {{CursorType::kNorthWestSouthEastResize,
      IDR_AURA_CURSOR_NORTH_WEST_SOUTH_EAST_RESIZE,
      {11, 11},
      {24, 23}}},
    {{CursorType::kColumnResize,
      IDR_AURA_CURSOR_COL_RESIZE,
      {12, 11},
      {25, 23}}},
    {{CursorType::kRowResize, IDR_AURA_CURSOR_ROW_RESIZE, {11, 12}, {23, 23}}},
    /*CursorType::kMiddlePanning*/ {},
    /*CursorType::kEastPanning*/ {},
    /*CursorType::kNorthPanning*/ {},
    /*CursorType::kNorthEastPanning*/ {},
    /*CursorType::kNorthWestPanning*/ {},
    /*CursorType::kSouthPanning*/ {},
    /*CursorType::kSouthEastPanning*/ {},
    /*CursorType::kSouthWestPanning*/ {},
    /*CursorType::kWestPanning*/ {},
    {{CursorType::kMove, IDR_AURA_CURSOR_MOVE, {11, 11}, {23, 23}}},
    {{CursorType::kVerticalText,
      IDR_AURA_CURSOR_XTERM_HORIZ,
      {12, 11},
      {26, 23}}},
    {{CursorType::kCell, IDR_AURA_CURSOR_CELL, {11, 11}, {24, 23}}},
    {{CursorType::kContextMenu, IDR_AURA_CURSOR_CONTEXT_MENU, {4, 4}, {8, 9}}},
    {{CursorType::kAlias, IDR_AURA_CURSOR_ALIAS, {8, 6}, {15, 11}}},
    {{CursorType::kProgress,
      IDR_AURA_CURSOR_THROBBER,
      {7, 7},
      {14, 14},
      /*is_animated=*/true}},
    {{CursorType::kNoDrop, IDR_AURA_CURSOR_NO_DROP, {9, 9}, {18, 18}}},
    {{CursorType::kCopy, IDR_AURA_CURSOR_COPY, {9, 9}, {18, 18}}},
    /*CursorType::kNone*/ {},
    {{CursorType::kNotAllowed, IDR_AURA_CURSOR_NO_DROP, {9, 9}, {18, 18}}},
    {{CursorType::kZoomIn, IDR_AURA_CURSOR_ZOOM_IN, {10, 10}, {20, 20}}},
    {{CursorType::kZoomOut, IDR_AURA_CURSOR_ZOOM_OUT, {10, 10}, {20, 20}}},
    {{CursorType::kGrab, IDR_AURA_CURSOR_GRAB, {8, 5}, {16, 10}}},
    {{CursorType::kGrabbing, IDR_AURA_CURSOR_GRABBING, {9, 9}, {18, 18}}},
    /*CursorType::kMiddlePanningVertical*/ {},
    /*CursorType::kMiddlePanningHorizontal*/ {},
    /*CursorType::kCustom*/ {},
    /*CursorType::kDndNone*/ {},
    /*CursorType::kDndMove*/ {},
    /*CursorType::kDndCopy*/ {},
    /*CursorType::kDndLink*/ {},
    {{CursorType::kEastWestNoResize,
      IDR_AURA_CURSOR_EAST_WEST_NO_RESIZE,
      {12, 11},
      {25, 23}}},
    {{CursorType::kNorthSouthNoResize,
      IDR_AURA_CURSOR_NORTH_SOUTH_NO_RESIZE,
      {11, 12},
      {23, 23}}},
    {{CursorType::kNorthEastSouthWestNoResize,
      IDR_AURA_CURSOR_NORTH_EAST_SOUTH_WEST_NO_RESIZE,
      {12, 11},
      {25, 23}}},
    {{CursorType::kNorthWestSouthEastNoResize,
      IDR_AURA_CURSOR_NORTH_WEST_SOUTH_EAST_NO_RESIZE,
      {11, 11},
      {24, 23}}},
};

static_assert(std::size(kNormalCursorResourceData) ==
              static_cast<int>(CursorType::kMaxValue) + 1);

constexpr std::optional<CursorResourceData> kLargeCursorResourceData[] = {
    {{CursorType::kPointer, IDR_AURA_CURSOR_BIG_PTR, {10, 10}, {20, 20}}},
    {{CursorType::kCross, IDR_AURA_CURSOR_BIG_CROSSHAIR, {30, 32}, {60, 64}}},
    {{CursorType::kHand, IDR_AURA_CURSOR_BIG_HAND, {25, 7}, {50, 14}}},
    {{CursorType::kIBeam, IDR_AURA_CURSOR_BIG_IBEAM, {30, 32}, {60, 64}}},
    {{CursorType::kWait,
      // TODO(crbug.com/40348660): create IDR_AURA_CURSOR_BIG_THROBBER.
      IDR_AURA_CURSOR_THROBBER,
      {7, 7},
      {14, 14},
      /*is_animated=*/true}},
    {{CursorType::kHelp, IDR_AURA_CURSOR_BIG_HELP, {10, 11}, {20, 22}}},
    {{CursorType::kEastResize,
      IDR_AURA_CURSOR_BIG_EAST_RESIZE,
      {35, 29},
      {70, 58}}},
    {{CursorType::kNorthResize,
      IDR_AURA_CURSOR_BIG_NORTH_RESIZE,
      {29, 32},
      {58, 64}}},
    {{CursorType::kNorthEastResize,
      IDR_AURA_CURSOR_BIG_NORTH_EAST_RESIZE,
      {31, 28},
      {62, 56}}},
    {{CursorType::kNorthWestResize,
      IDR_AURA_CURSOR_BIG_NORTH_WEST_RESIZE,
      {28, 28},
      {56, 56}}},
    {{CursorType::kSouthResize,
      IDR_AURA_CURSOR_BIG_SOUTH_RESIZE,
      {29, 32},
      {58, 64}}},
    {{CursorType::kSouthEastResize,
      IDR_AURA_CURSOR_BIG_SOUTH_EAST_RESIZE,
      {28, 28},
      {56, 56}}},
    {{CursorType::kSouthWestResize,
      IDR_AURA_CURSOR_BIG_SOUTH_WEST_RESIZE,
      {31, 28},
      {62, 56}}},
    {{CursorType::kWestResize,
      IDR_AURA_CURSOR_BIG_WEST_RESIZE,
      {35, 29},
      {70, 58}}},
    {{CursorType::kNorthSouthResize,
      IDR_AURA_CURSOR_BIG_NORTH_SOUTH_RESIZE,
      {29, 32},
      {58, 64}}},
    {{CursorType::kEastWestResize,
      IDR_AURA_CURSOR_BIG_EAST_WEST_RESIZE,
      {35, 29},
      {70, 58}}},
    {{CursorType::kNorthEastSouthWestResize,
      IDR_AURA_CURSOR_BIG_NORTH_EAST_SOUTH_WEST_RESIZE,
      {32, 30},
      {64, 60}}},
    {{CursorType::kNorthWestSouthEastResize,
      IDR_AURA_CURSOR_BIG_NORTH_WEST_SOUTH_EAST_RESIZE,
      {32, 31},
      {64, 62}}},
    {{CursorType::kColumnResize,
      IDR_AURA_CURSOR_BIG_COL_RESIZE,
      {35, 29},
      {70, 58}}},
    {{CursorType::kRowResize,
      IDR_AURA_CURSOR_BIG_ROW_RESIZE,
      {29, 32},
      {58, 64}}},
    /*CursorType::kMiddlePanning*/ {},
    /*CursorType::kEastPanning*/ {},
    /*CursorType::kNorthPanning*/ {},
    /*CursorType::kNorthEastPanning*/ {},
    /*CursorType::kNorthWestPanning*/ {},
    /*CursorType::kSouthPanning*/ {},
    /*CursorType::kSouthEastPanning*/ {},
    /*CursorType::kSouthWestPanning*/ {},
    /*CursorType::kWestPanning*/ {},
    {{CursorType::kMove, IDR_AURA_CURSOR_BIG_MOVE, {32, 31}, {64, 62}}},
    {{CursorType::kVerticalText,
      IDR_AURA_CURSOR_BIG_XTERM_HORIZ,
      {32, 30},
      {64, 60}}},
    {{CursorType::kCell, IDR_AURA_CURSOR_BIG_CELL, {30, 30}, {60, 60}}},
    {{CursorType::kContextMenu,
      IDR_AURA_CURSOR_BIG_CONTEXT_MENU,
      {11, 11},
      {22, 22}}},
    {{CursorType::kAlias, IDR_AURA_CURSOR_BIG_ALIAS, {19, 11}, {38, 22}}},
    {{CursorType::kProgress,
      // TODO(crbug.com/40348660): create IDR_AURA_CURSOR_BIG_THROBBER.
      IDR_AURA_CURSOR_THROBBER,
      {7, 7},
      {14, 14},
      /*is_animated=*/true}},
    {{CursorType::kNoDrop, IDR_AURA_CURSOR_BIG_NO_DROP, {10, 10}, {20, 20}}},
    {{CursorType::kCopy, IDR_AURA_CURSOR_BIG_COPY, {21, 11}, {42, 22}}},
    /*CursorType::kNone*/ {},
    {{CursorType::kNotAllowed,
      IDR_AURA_CURSOR_BIG_NO_DROP,
      {10, 10},
      {20, 20}}},
    {{CursorType::kZoomIn, IDR_AURA_CURSOR_BIG_ZOOM_IN, {25, 26}, {50, 52}}},
    {{CursorType::kZoomOut, IDR_AURA_CURSOR_BIG_ZOOM_OUT, {26, 26}, {52, 52}}},
    {{CursorType::kGrab, IDR_AURA_CURSOR_BIG_GRAB, {21, 11}, {42, 22}}},
    {{CursorType::kGrabbing, IDR_AURA_CURSOR_BIG_GRABBING, {20, 12}, {40, 24}}},
    /*CursorType::kMiddlePanningVertical*/ {},
    /*CursorType::kMiddlePanningHorizontal*/ {},
    /*CursorType::kCustom*/ {},
    /*CursorType::kDndNone*/ {},
    /*CursorType::kDndMove*/ {},
    /*CursorType::kDndCopy*/ {},
    /*CursorType::kDndLink*/ {},
    {{CursorType::kEastWestNoResize,
      IDR_AURA_CURSOR_BIG_EAST_WEST_NO_RESIZE,
      {35, 29},
      {70, 58}}},
    {{CursorType::kNorthSouthNoResize,
      IDR_AURA_CURSOR_BIG_NORTH_SOUTH_NO_RESIZE,
      {29, 32},
      {58, 64}}},
    {{CursorType::kNorthEastSouthWestNoResize,
      IDR_AURA_CURSOR_BIG_NORTH_EAST_SOUTH_WEST_NO_RESIZE,
      {32, 30},
      {64, 60}}},
    {{CursorType::kNorthWestSouthEastNoResize,
      IDR_AURA_CURSOR_BIG_NORTH_WEST_SOUTH_EAST_NO_RESIZE,
      {32, 31},
      {64, 62}}},
};

static_assert(std::size(kLargeCursorResourceData) ==
              static_cast<int>(CursorType::kMaxValue) + 1);

}  // namespace

std::optional<ui::CursorData> GetCursorData(
    CursorType type,
    ui::CursorSize size,
    float scale,
    std::optional<int> target_cursor_size_in_px,
    display::Display::Rotation rotation) {
  DCHECK_NE(type, CursorType::kCustom);

  int resource_id;
  gfx::Point hotspot;
  bool is_animated;
  if (!GetCursorDataFor(size, type, scale, &resource_id, &hotspot,
                        &is_animated)) {
    return std::nullopt;
  }
  DCHECK_NE(type, CursorType::kNone);

  std::vector<SkBitmap> bitmaps;
  const gfx::ImageSkia* image =
      ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(resource_id);
  const float resource_scale = ui::GetScaleForResourceScaleFactor(
      ui::GetSupportedResourceScaleFactorForRescale(scale));
  const gfx::ImageSkiaRep& image_rep = image->GetRepresentation(resource_scale);
  CHECK_EQ(image_rep.scale(), resource_scale);

  if (target_cursor_size_in_px) {
    // If `target_cursor_size_in_px` presents, use it to calculate scale.
    // Use `image_rep.GetHeight()` as cursor dp size. An animated bitmap
    // is composed of horizontally tiled frames so its width could not
    // be used as cursor size.
    int cursor_size_in_dp = image_rep.GetHeight();
    scale = static_cast<float>(target_cursor_size_in_px.value()) /
            static_cast<float>(cursor_size_in_dp);
  }

  if (!image_rep.unscaled()) {
    // Bitmap-based cursor image.
    CreateBitmapsFromPixels(image_rep, scale, rotation, is_animated, &bitmaps,
                            &hotspot);
  } else {
    const gfx::Size scaled_size = ScaleToRoundedSize(
        gfx::Size(image_rep.GetWidth(), image_rep.GetHeight()), scale);
    const gfx::Transform rotation_transform = display::CreateRotationTransform(
        rotation, gfx::SizeF(scaled_size.width(), scaled_size.height()));
    hotspot = gfx::ScaleToFlooredPoint(hotspot, scale);
    RotateCursorHotpoint(rotation, scaled_size.width(), scaled_size.height(),
                         &hotspot);

    if (!is_animated) {
      // Non-animated lottie cursor.
      CreateBitmapsFromStaticLottie(image_rep, scaled_size, scale,
                                    rotation_transform, &bitmaps);
    } else {
      // Animated lottie cursor.
      CreateBitmapsFromAnimatedLottie(resource_id, scaled_size, scale,
                                      rotation_transform, type, &bitmaps);
    }
  }

  return ui::CursorData(std::move(bitmaps), std::move(hotspot), scale);
}

void ScaleAndRotateCursorBitmapAndHotpoint(float scale,
                                           display::Display::Rotation rotation,
                                           SkBitmap* bitmap,
                                           gfx::Point* hotpoint) {
  if (scale < FLT_EPSILON) {
    NOTREACHED_IN_MIGRATION() << "Scale must be larger than 0.";
    scale = 1.0f;
  }

  // SkBitmapOperations::Rotate() and skia::ImageOperations::Resize()
  // need the bitmap to have premultiplied alpha, so convert bitmap alpha type
  // if we are going to rotate or scale.
  bool was_converted = false;
  if ((rotation != display::Display::ROTATE_0 || scale != 1.0f) &&
      bitmap->info().alphaType() == kUnpremul_SkAlphaType) {
    ConvertSkBitmapAlphaType(bitmap, kPremul_SkAlphaType);
    was_converted = true;
  }

  RotateCursorBitmapAndHotpoint(rotation, bitmap, hotpoint);

  if (scale == 1.0f) {
    if (was_converted) {
      ConvertSkBitmapAlphaType(bitmap, kUnpremul_SkAlphaType);
    }
    return;
  }

  gfx::Size scaled_size = gfx::ScaleToFlooredSize(
      gfx::Size(bitmap->width(), bitmap->height()), scale);
  SkBitmap scaled_bitmap;
  // Use RESIZE_BEST to avoid blurry large cursor on external displays.
  // See crbug.com/1229231.
  scaled_bitmap =
      skia::ImageOperations::Resize(*bitmap, skia::ImageOperations::RESIZE_BEST,
                                    scaled_size.width(), scaled_size.height());

  if (was_converted) {
    ConvertSkBitmapAlphaType(&scaled_bitmap, kUnpremul_SkAlphaType);
  }

  *bitmap = scaled_bitmap;
  if (hotpoint) {
    *hotpoint = gfx::ScaleToFlooredPoint(*hotpoint, scale);
  }
}

bool GetCursorDataFor(ui::CursorSize cursor_size,
                      CursorType type,
                      float scale_factor,
                      int* resource_id,
                      gfx::Point* point,
                      bool* is_animated) {
  DCHECK_NE(type, CursorType::kCustom);

  // TODO(https://crbug.com/1270302: temporary check until GetCursorDataFor is
  // replaced by GetCursorData, which is only used internally by CursorLoader.
  if (type == CursorType::kNone) {
    return false;
  }

  // TODO(htts://crbug.com/1190818): currently, kNull is treated as kPointer.
  CursorType t = type == CursorType::kNull ? CursorType::kPointer : type;
  std::optional<CursorResourceData> resource =
      cursor_size == ui::CursorSize::kNormal
          ? kNormalCursorResourceData[static_cast<int>(t)]
          : kLargeCursorResourceData[static_cast<int>(t)];
  if (!resource) {
    return false;
  }

  DCHECK_EQ(resource->type, t);
  *resource_id = resource->id;
  *point = resource->hotspot_1x;
  if (ui::GetSupportedResourceScaleFactorForRescale(scale_factor) ==
      ui::k200Percent) {
    *point = resource->hotspot_2x;
  }
  *is_animated = resource->is_animated;
  return true;
}

}  // namespace wm
