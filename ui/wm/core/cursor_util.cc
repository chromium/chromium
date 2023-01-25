// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/wm/core/cursor_util.h"

#include "base/check_op.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/cursor_size.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "ui/gfx/skbitmap_operations.h"
#include "ui/resources/grit/ui_resources.h"

namespace wm {

namespace {

using ::ui::mojom::CursorType;

constexpr CursorType kAnimatedCursorTypes[] = {CursorType::kWait,
                                               CursorType::kProgress};

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

void GetImageCursorBitmap(int resource_id,
                          float scale,
                          display::Display::Rotation rotation,
                          gfx::Point* hotspot,
                          SkBitmap* bitmap) {
  const gfx::ImageSkia* image =
      ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(resource_id);
  const gfx::ImageSkiaRep& image_rep = image->GetRepresentation(scale);
  // TODO(oshima): The cursor should use resource scale factor when
  // fractional scale factor is enabled. crbug.com/372212
  (*bitmap) = image_rep.GetBitmap();
  ScaleAndRotateCursorBitmapAndHotpoint(scale / image_rep.scale(), rotation,
                                        bitmap, hotspot);
  // |image_rep| is owned by the resource bundle. So we do not need to free it.
}

void GetAnimatedCursorBitmaps(int resource_id,
                              float scale,
                              display::Display::Rotation rotation,
                              gfx::Point* hotspot,
                              std::vector<SkBitmap>* bitmaps) {
  // TODO(oshima|tdanderson): Support rotation and fractional scale factor.
  const gfx::ImageSkia* image =
      ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(resource_id);
  const gfx::ImageSkiaRep& image_rep = image->GetRepresentation(scale);
  SkBitmap bitmap = image_rep.GetBitmap();

  // The image is assumed to be a concatenation of animation frames from left to
  // right. Also, each frame is assumed to be square (width == height).
  int frame_width = bitmap.height();
  int frame_height = frame_width;
  int total_width = bitmap.width();
  DCHECK_EQ(total_width % frame_width, 0);
  int frame_count = total_width / frame_width;
  DCHECK_GT(frame_count, 0);

  bitmaps->resize(frame_count);

  for (int frame = 0; frame < frame_count; ++frame) {
    int x_offset = frame_width * frame;
    DCHECK_LE(x_offset + frame_width, total_width);

    SkBitmap cropped = SkBitmapOperations::CreateTiledBitmap(
        bitmap, x_offset, 0, frame_width, frame_height);
    DCHECK_EQ(frame_width, cropped.width());
    DCHECK_EQ(frame_height, cropped.height());

    (*bitmaps)[frame] = cropped;
  }
}

struct CursorResourceData {
  CursorType type;
  int id;
  gfx::Point hotspot_1x;
  gfx::Point hotspot_2x;
};

// Cursor resource data indexed by CursorType. Make sure to respect the order
// defined at ui/base/cursor/mojom/cursor_type.mojom.
constexpr absl::optional<CursorResourceData> kNormalCursorResourceData[] = {
    {{CursorType::kPointer, IDR_AURA_CURSOR_PTR, {4, 4}, {7, 7}}},
    {{CursorType::kCross, IDR_AURA_CURSOR_CROSSHAIR, {12, 12}, {24, 24}}},
    {{CursorType::kHand, IDR_AURA_CURSOR_HAND, {9, 4}, {19, 8}}},
    {{CursorType::kIBeam, IDR_AURA_CURSOR_IBEAM, {12, 12}, {24, 25}}},
    {{CursorType::kWait, IDR_AURA_CURSOR_THROBBER, {7, 7}, {14, 14}}},
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
    {{CursorType::kProgress, IDR_AURA_CURSOR_THROBBER, {7, 7}, {14, 14}}},
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

constexpr absl::optional<CursorResourceData> kLargeCursorResourceData[] = {
    {{CursorType::kPointer, IDR_AURA_CURSOR_BIG_PTR, {10, 10}, {20, 20}}},
    {{CursorType::kCross, IDR_AURA_CURSOR_BIG_CROSSHAIR, {30, 32}, {60, 64}}},
    {{CursorType::kHand, IDR_AURA_CURSOR_BIG_HAND, {25, 7}, {50, 14}}},
    {{CursorType::kIBeam, IDR_AURA_CURSOR_BIG_IBEAM, {30, 32}, {60, 64}}},
    {{CursorType::kWait,
      // TODO(https://crbug.com/336867): create IDR_AURA_CURSOR_BIG_THROBBER.
      IDR_AURA_CURSOR_THROBBER,
      {7, 7},
      {14, 14}}},
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
      // TODO(https://crbug.com/336867): create IDR_AURA_CURSOR_BIG_THROBBER.
      IDR_AURA_CURSOR_THROBBER,
      {7, 7},
      {14, 14}}},
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

absl::optional<ui::CursorData> GetCursorData(
    CursorType type,
    ui::CursorSize size,
    float scale,
    display::Display::Rotation rotation) {
  DCHECK_NE(type, CursorType::kNone);
  DCHECK_NE(type, CursorType::kCustom);

  int resource_id;
  gfx::Point hotspot;
  if (!GetCursorDataFor(size, type, scale, &resource_id, &hotspot)) {
    return absl::nullopt;
  }

  std::vector<SkBitmap> bitmaps;
  if (base::ranges::count(kAnimatedCursorTypes, type) == 0) {
    SkBitmap bitmap;
    GetImageCursorBitmap(resource_id, scale, rotation, &hotspot, &bitmap);
    bitmaps.push_back(std::move(bitmap));
  } else {
    GetAnimatedCursorBitmaps(resource_id, scale, rotation, &hotspot, &bitmaps);
  }
  return ui::CursorData(std::move(bitmaps), std::move(hotspot));
}

void ScaleAndRotateCursorBitmapAndHotpoint(float scale,
                                           display::Display::Rotation rotation,
                                           SkBitmap* bitmap,
                                           gfx::Point* hotpoint) {
  // SkBitmapOperations::Rotate() needs the bitmap to have premultiplied alpha,
  // so convert bitmap alpha type if we are going to rotate.
  bool was_converted = false;
  if (rotation != display::Display::ROTATE_0 &&
      bitmap->info().alphaType() == kUnpremul_SkAlphaType) {
    ConvertSkBitmapAlphaType(bitmap, kPremul_SkAlphaType);
    was_converted = true;
  }

  switch (rotation) {
    case display::Display::ROTATE_0:
      break;
    case display::Display::ROTATE_90:
      hotpoint->SetPoint(bitmap->height() - hotpoint->y(), hotpoint->x());
      *bitmap = SkBitmapOperations::Rotate(
          *bitmap, SkBitmapOperations::ROTATION_90_CW);
      break;
    case display::Display::ROTATE_180:
      hotpoint->SetPoint(
          bitmap->width() - hotpoint->x(), bitmap->height() - hotpoint->y());
      *bitmap = SkBitmapOperations::Rotate(
          *bitmap, SkBitmapOperations::ROTATION_180_CW);
      break;
    case display::Display::ROTATE_270:
      hotpoint->SetPoint(hotpoint->y(), bitmap->width() - hotpoint->x());
      *bitmap = SkBitmapOperations::Rotate(
          *bitmap, SkBitmapOperations::ROTATION_270_CW);
      break;
  }

  if (was_converted) {
    ConvertSkBitmapAlphaType(bitmap, kUnpremul_SkAlphaType);
  }

  if (scale < FLT_EPSILON) {
    NOTREACHED() << "Scale must be larger than 0.";
    scale = 1.0f;
  }

  if (scale == 1.0f)
    return;

  gfx::Size scaled_size = gfx::ScaleToFlooredSize(
      gfx::Size(bitmap->width(), bitmap->height()), scale);

  // TODO(crbug.com/919866): skia::ImageOperations::Resize() doesn't support
  // unpremultiplied alpha bitmaps.
  SkBitmap scaled_bitmap;
  scaled_bitmap.setInfo(
      bitmap->info().makeWH(scaled_size.width(), scaled_size.height()));
  if (scaled_bitmap.tryAllocPixels()) {
    bitmap->pixmap().scalePixels(
        scaled_bitmap.pixmap(),
        {SkFilterMode::kLinear, SkMipmapMode::kNearest});
  }

  *bitmap = scaled_bitmap;
  *hotpoint = gfx::ScaleToFlooredPoint(*hotpoint, scale);
}

bool GetCursorDataFor(ui::CursorSize cursor_size,
                      CursorType type,
                      float scale_factor,
                      int* resource_id,
                      gfx::Point* point) {
  DCHECK_NE(type, CursorType::kCustom);

  // TODO(https://crbug.com/1270302: temporary check until GetCursorDataFor is
  // replaced by GetCursorData, which is only used internally by CursorLoader.
  if (type == CursorType::kNone) {
    return false;
  }

  // TODO(htts://crbug.com/1190818): currently, kNull is treated as kPointer.
  CursorType t = type == CursorType::kNull ? CursorType::kPointer : type;
  absl::optional<CursorResourceData> resource =
      cursor_size == ui::CursorSize::kNormal
          ? kNormalCursorResourceData[static_cast<int>(t)]
          : kLargeCursorResourceData[static_cast<int>(t)];
  if (!resource) {
    return false;
  }

  DCHECK_EQ(resource->type, t);
  *resource_id = resource->id;
  *point = resource->hotspot_1x;
  bool resource_2x_available =
      ui::ResourceBundle::GetSharedInstance().GetMaxResourceScaleFactor() ==
      ui::k200Percent;
  if (scale_factor != 1.0f && resource_2x_available) {
    *point = resource->hotspot_2x;
  }
  return true;
}

}  // namespace wm
