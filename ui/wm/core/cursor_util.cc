// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/wm/core/cursor_util.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <memory>
#include <optional>

#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "cc/paint/skottie_wrapper.h"
#include "skia/ext/image_operations.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/cursor/cursor.h"
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
#include "ui/resources/grit/ui_lottie_resources.h"
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
  UNSAFE_TODO(memcpy(bitmap->getPixels(), &buffer[0], buffer.size()));

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

// Create a bitmap from a lottie `animation` at the normalized time instance `t`
// with given `rotation_transform`, `scale` and `scaled_size`.
SkBitmap CreateBitmapFromLottieAnimation(
    lottie::Animation* animation,
    float t,
    const gfx::Size& scaled_size,
    float scale,
    const gfx::Transform& rotation_transform) {
  // Non-animated cursor.
  SkBitmap bitmap;
  bitmap.allocN32Pixels(scaled_size.width(), scaled_size.height());
  bitmap.eraseColor(SK_ColorTRANSPARENT);

  cc::SkiaPaintCanvas paint_canvas(bitmap);
  gfx::Canvas canvas(&paint_canvas, scale);
  canvas.Transform(rotation_transform);
  animation->PaintFrame(&canvas, t, scaled_size);
  bitmap.setImmutable();

  return bitmap;
}

struct CursorResourceData {
  CursorType type;
  int id;
  // The hotspot (in DIP) of the cursor.
  gfx::Point hotspot;
  bool is_animated = false;
};

// Cursor resource data indexed by CursorType. Make sure to respect the order
// defined at ui/base/cursor/mojom/cursor_type.mojom.
constexpr auto kCursorResourceData = std::to_array<
    std::optional<CursorResourceData>>({
    {{CursorType::kPointer, IDR_AURA_CURSOR_PTR_LOTTIE, {6, 4}}},
    {{CursorType::kCross, IDR_AURA_CURSOR_CROSSHAIR_LOTTIE, {12, 12}}},
    {{CursorType::kHand, IDR_AURA_CURSOR_HAND_LOTTIE, {9, 3}}},
    {{CursorType::kIBeam, IDR_AURA_CURSOR_IBEAM_LOTTIE, {12, 12}}},
    {{CursorType::kWait,
      IDR_AURA_CURSOR_THROBBER_LOTTIE,
      {12, 12},
      /*is_animated=*/true}},
    {{CursorType::kHelp, IDR_AURA_CURSOR_HELP_LOTTIE, {4, 4}}},
    {{CursorType::kEastResize,
      IDR_AURA_CURSOR_HORIZONTAL_DOUBLE_ARROW_LOTTIE,
      {12, 12}}},
    {{CursorType::kNorthResize,
      IDR_AURA_CURSOR_VERTICAL_DOUBLE_ARROW_LOTTIE,
      {12, 12}}},
    {{CursorType::kNorthEastResize,
      IDR_AURA_CURSOR_TOP_RIGHT_CORNER_LOTTIE,
      {12, 13}}},
    {{CursorType::kNorthWestResize,
      IDR_AURA_CURSOR_TOP_LEFT_CORNER_LOTTIE,
      {12, 12}}},
    {{CursorType::kSouthResize,
      IDR_AURA_CURSOR_VERTICAL_DOUBLE_ARROW_LOTTIE,
      {12, 12}}},
    {{CursorType::kSouthEastResize,
      IDR_AURA_CURSOR_TOP_LEFT_CORNER_LOTTIE,
      {12, 12}}},
    {{CursorType::kSouthWestResize,
      IDR_AURA_CURSOR_TOP_RIGHT_CORNER_LOTTIE,
      {12, 13}}},
    {{CursorType::kWestResize,
      IDR_AURA_CURSOR_HORIZONTAL_DOUBLE_ARROW_LOTTIE,
      {12, 12}}},
    {{CursorType::kNorthSouthResize,
      IDR_AURA_CURSOR_VERTICAL_DOUBLE_ARROW_LOTTIE,
      {12, 12}}},
    {{CursorType::kEastWestResize,
      IDR_AURA_CURSOR_HORIZONTAL_DOUBLE_ARROW_LOTTIE,
      {12, 12}}},
    {{CursorType::kNorthEastSouthWestResize,
      IDR_AURA_CURSOR_TOP_RIGHT_CORNER_LOTTIE,
      {12, 13}}},
    {{CursorType::kNorthWestSouthEastResize,
      IDR_AURA_CURSOR_TOP_LEFT_CORNER_LOTTIE,
      {12, 12}}},
    {{CursorType::kColumnResize,
      IDR_AURA_CURSOR_HORIZONTAL_DOUBLE_ARROW_LOTTIE,
      {12, 12}}},
    {{CursorType::kRowResize,
      IDR_AURA_CURSOR_VERTICAL_DOUBLE_ARROW_LOTTIE,
      {12, 12}}},
    /*CursorType::kMiddlePanning*/ {},
    /*CursorType::kEastPanning*/ {},
    /*CursorType::kNorthPanning*/ {},
    /*CursorType::kNorthEastPanning*/ {},
    /*CursorType::kNorthWestPanning*/ {},
    /*CursorType::kSouthPanning*/ {},
    /*CursorType::kSouthEastPanning*/ {},
    /*CursorType::kSouthWestPanning*/ {},
    /*CursorType::kWestPanning*/ {},
    {{CursorType::kMove, IDR_AURA_CURSOR_MOVE_LOTTIE, {12, 12}}},
    {{CursorType::kVerticalText, IDR_AURA_CURSOR_XTERM_HORIZ_LOTTIE, {12, 12}}},
    {{CursorType::kCell, IDR_AURA_CURSOR_CELL_LOTTIE, {12, 12}}},
    {{CursorType::kContextMenu, IDR_AURA_CURSOR_CONTEXT_MENU_LOTTIE, {3, 4}}},
    {{CursorType::kAlias, IDR_AURA_CURSOR_ALIAS_LOTTIE, {6, 4}}},
    {{CursorType::kProgress,
      IDR_AURA_CURSOR_THROBBER_LOTTIE,
      {12, 12},
      /*is_animated=*/true}},
    {{CursorType::kNoDrop, IDR_AURA_CURSOR_NO_DROP_LOTTIE, {8, 7}}},
    {{CursorType::kCopy, IDR_AURA_CURSOR_COPY_LOTTIE, {8, 7}}},
    /*CursorType::kNone*/ {},
    {{CursorType::kNotAllowed, IDR_AURA_CURSOR_NO_DROP_LOTTIE, {8, 7}}},
    {{CursorType::kZoomIn, IDR_AURA_CURSOR_ZOOM_IN_LOTTIE, {10, 10}}},
    {{CursorType::kZoomOut, IDR_AURA_CURSOR_ZOOM_OUT_LOTTIE, {10, 10}}},
    {{CursorType::kGrab, IDR_AURA_CURSOR_GRAB_LOTTIE, {8, 4}}},
    {{CursorType::kGrabbing, IDR_AURA_CURSOR_GRABBING_LOTTIE, {8, 5}}},
    /*CursorType::kMiddlePanningVertical*/ {},
    /*CursorType::kMiddlePanningHorizontal*/ {},
    /*CursorType::kCustom*/ {},
    /*CursorType::kDndNone*/ {},
    /*CursorType::kDndMove*/ {},
    /*CursorType::kDndCopy*/ {},
    /*CursorType::kDndLink*/ {},
    {{CursorType::kEastWestNoResize,
      IDR_AURA_CURSOR_HORIZONTAL_DOUBLE_ARROW_BLOCK_LOTTIE,
      {12, 14}}},
    {{CursorType::kNorthSouthNoResize,
      IDR_AURA_CURSOR_VERTICAL_DOUBLE_ARROW_BLOCK_LOTTIE,
      {10, 13}}},
    {{CursorType::kNorthEastSouthWestNoResize,
      IDR_AURA_CURSOR_TOP_RIGHT_CORNER_BLOCK_LOTTIE,
      {14, 14}}},
    {{CursorType::kNorthWestSouthEastNoResize,
      IDR_AURA_CURSOR_TOP_LEFT_CORNER_BLOCK_LOTTIE,
      {10, 14}}},
});

static_assert(std::size(kCursorResourceData) ==
              static_cast<int>(CursorType::kMaxValue) + 1);

// The name of cursor fill shape in lottie.
constexpr char kCursorFillColorName[] = "cursor.fill.color";

// Target frame rate for animated cursor.
constexpr int kAnimatedCursorFramePerSecond = 60;

}  // namespace

std::optional<ui::CursorData> GetCursorData(
    CursorType type,
    float scale,
    std::optional<int> target_cursor_size_in_px,
    display::Display::Rotation rotation,
    SkColor color) {
  DCHECK_NE(type, CursorType::kCustom);

  int resource_id;
  gfx::Point hotspot;
  bool is_animated;
  if (!GetCursorDataFor(type, &resource_id, &hotspot, &is_animated)) {
    return std::nullopt;
  }
  DCHECK_NE(type, CursorType::kNone);

  cc::SkottieColorMap colormap = {};
  if (color != ui::kDefaultCursorColor) {
    // If cursor color is not the default, color map needs to be applied
    // when creating lottie animation for dynamic coloration.
    colormap.emplace(cc::SkottieMapColor(kCursorFillColorName, color));
  }

  AnimationCache& cursor_animations = GetAnimationCache();
  if (!base::Contains(cursor_animations, type)) {
    // Read lottie content and create a lottie animation.
    std::optional<std::vector<uint8_t>> lottie_bytes =
        ui::ResourceBundle::GetSharedInstance().GetLottieData(resource_id);
    scoped_refptr<cc::SkottieWrapper> skottie =
        cc::SkottieWrapper::UnsafeCreateSerializable(std::move(*lottie_bytes));
    cursor_animations[type] =
        std::make_unique<lottie::Animation>(skottie, colormap);
  }
  lottie::Animation* animation = cursor_animations[type].get();

  const gfx::Size cursor_size_in_dp = animation->GetOriginalSize();
  if (target_cursor_size_in_px) {
    // If `target_cursor_size_in_px` presents, use it to calculate scale.
    // Use `image_rep.GetHeight()` as cursor dp size. An animated bitmap
    // is composed of horizontally tiled frames so its width could not
    // be used as cursor size.
    scale = static_cast<float>(target_cursor_size_in_px.value()) /
            static_cast<float>(cursor_size_in_dp.height());
  }
  const gfx::Size scaled_size = ScaleToRoundedSize(cursor_size_in_dp, scale);

  hotspot = gfx::ScaleToFlooredPoint(hotspot, scale);
  RotateCursorHotpoint(rotation, scaled_size.width(), scaled_size.height(),
                       &hotspot);

  const gfx::Transform rotation_transform = display::CreateRotationTransform(
      rotation, gfx::SizeF(scaled_size.width(), scaled_size.height()));

  std::vector<SkBitmap> bitmaps;

  const int frames = is_animated
                         ? kAnimatedCursorFramePerSecond *
                               animation->GetAnimationDuration().InSecondsF()
                         : 1;
  for (int i = 0; i < frames; i++) {
    float t = static_cast<float>(i) / frames;
    bitmaps.push_back(CreateBitmapFromLottieAnimation(
        animation, t, scaled_size, scale, rotation_transform));
  }

  return ui::CursorData(std::move(bitmaps), std::move(hotspot), scale);
}

void ScaleAndRotateCursorBitmapAndHotpoint(float scale,
                                           display::Display::Rotation rotation,
                                           SkBitmap* bitmap,
                                           gfx::Point* hotpoint) {
  if (scale < FLT_EPSILON) {
    NOTREACHED() << "Scale must be larger than 0.";
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

bool GetCursorDataFor(CursorType type,
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
  const std::optional<CursorResourceData>& resource =
      kCursorResourceData[static_cast<int>(t)];
  if (!resource) {
    return false;
  }

  DCHECK_EQ(resource->type, t);
  *resource_id = resource->id;
  *point = resource->hotspot;
  *is_animated = resource->is_animated;
  return true;
}

SkBitmap GetColorAdjustedBitmap(const SkBitmap& bitmap, SkColor cursor_color) {
  // Recolor the black and greyscale parts of the image based on
  // `cursor_color`. Do not recolor pure white or tinted portions of the image,
  // this ensures we do not impact the colored portions of cursors or the
  // transition between the colored portion and white outline.
  // TODO(b:376929449): Programmatically find a way to recolor the white
  // parts in order to draw a black outline, but without impacting cursors
  // like noDrop which contained tinted portions. Or, add new assets with
  // black and white inverted for easier re-coloring.
  SkBitmap recolored;
  recolored.allocN32Pixels(bitmap.width(), bitmap.height());
  recolored.eraseARGB(0, 0, 0, 0);
  SkCanvas canvas(recolored);
  canvas.drawImage(bitmap.asImage(), 0, 0);
  color_utils::HSL cursor_hsl;
  color_utils::SkColorToHSL(cursor_color, &cursor_hsl);
  for (int y = 0; y < bitmap.height(); ++y) {
    for (int x = 0; x < bitmap.width(); ++x) {
      const SkColor color = bitmap.getColor(x, y);
      // If the alpha is lower than 1, it's transparent, skip it.
      if (SkColorGetA(color) < 1) {
        continue;
      }
      // Convert to HSL: We want to change the hue and saturation, and
      // map the lightness from 0-100 to cursor_hsl.l-100. This means that
      // things which were black (l=0) become the cursor color lightness, and
      // things which were white (l=100) stay white.
      color_utils::HSL hsl;
      color_utils::SkColorToHSL(color, &hsl);
      // If it has color, do not change it.
      if (hsl.s > 0.01) {
        continue;
      }
      color_utils::HSL result;
      result.h = cursor_hsl.h;
      result.s = cursor_hsl.s;
      result.l = hsl.l * (1 - cursor_hsl.l) + cursor_hsl.l;
      SkPaint paint;
      paint.setColor(color_utils::HSLToSkColor(result, SkColorGetA(color)));
      canvas.drawRect(SkRect::MakeXYWH(x, y, 1, 1), paint);
    }
  }
  return recolored;
}

void ClearCursorAnimationCache() {
  GetAnimationCache().clear();
}

}  // namespace wm
