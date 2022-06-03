// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/corewm/cursor_height_provider_win.h"

#include <stddef.h>
#include <stdint.h>
#include <windows.h>

#include <algorithm>
#include <map>
#include <memory>

#include "base/win/scoped_hdc.h"

namespace {
using PixelData = std::unique_ptr<uint32_t[]>;
using HeightStorage = std::map<HCURSOR, int>;

const uint32_t kBitsPeruint32 = sizeof(uint32_t) * 8;
// All bits are 1 for transparent portion of monochromatic mask.
constexpr uint32_t kTransparentMask = 0xffffffff;
// This is height of default pointer arrow in Windows 7.
constexpr int kDefaultHeight = 20;
// Masks are monochromatic.
constexpr size_t kNumberOfColors = 2;
const size_t kHeaderAndPalette =
    sizeof(BITMAPINFOHEADER) + kNumberOfColors * sizeof(RGBQUAD);

HeightStorage* cached_heights = nullptr;

// Extracts the pixel data of provided bitmap
PixelData GetBitmapData(HBITMAP handle, const BITMAPINFO& info, HDC hdc) {
  PixelData data;
  // Masks are monochromatic.
  DCHECK_EQ(info.bmiHeader.biBitCount, 1);
  if (info.bmiHeader.biBitCount != 1)
    return data;

  // When getting pixel data palette is appended to memory pointed by
  // BITMAPINFO passed so allocate additional memory to store additional data.
  auto header = std::make_unique<char[]>(kHeaderAndPalette);
  memcpy(header.get(), &(info.bmiHeader), sizeof(info.bmiHeader));

  data = std::make_unique<uint32_t[]>(info.bmiHeader.biSizeImage /
                                      sizeof(uint32_t));

  int result =
      GetDIBits(hdc, handle, 0, info.bmiHeader.biHeight, data.get(),
                reinterpret_cast<BITMAPINFO*>(header.get()), DIB_RGB_COLORS);

  if (result == 0)
    data.reset();

  return data;
}

// Checks if the specifed row is transparent in provided bitmap.
bool IsRowTransparent(const PixelData& data,
                      const uint32_t row_size,
                      const uint32_t last_byte_mask,
                      const uint32_t y) {
  // Set the padding bits to 1 to make mask matching easier.
  *(data.get() + (y + 1) * row_size - 1) |= last_byte_mask;
  for (uint32_t i = y * row_size; i < (y + 1) * row_size; ++i) {
    if (*(data.get() + i) != kTransparentMask)
      return false;
  }
  return true;
}

// Gets the vertical offset between specified cursor's hotpoint and it's bottom.
//
// Gets the cursor image data and extract cursor's visible height.
// Based on that get's what should be the vertical offset between cursor's
// hot point and the tooltip.
int CalculateCursorHeight(HCURSOR cursor_handle) {
  base::win::ScopedGetDC hdc(nullptr);

  ICONINFO icon = {0};
  GetIconInfo(cursor_handle, &icon);

  BITMAPINFO bitmap_info = {};
  bitmap_info.bmiHeader.biSize = sizeof(bitmap_info.bmiHeader);
  if (GetDIBits(hdc, icon.hbmMask, 0, 0, nullptr, &bitmap_info,
                DIB_RGB_COLORS) == 0)
    return kDefaultHeight;

  // Rows are padded to full DWORDs. OR with this mask will set them to 1
  // to simplify matching with |transparent_mask|.
  uint32_t last_byte_mask = 0xFFFFFFFF;
  const unsigned char bits_to_shift =
      sizeof(last_byte_mask) * 8 -
      (bitmap_info.bmiHeader.biWidth % kBitsPeruint32);
  if (bits_to_shift != kBitsPeruint32)
    last_byte_mask = (last_byte_mask << bits_to_shift);
  else
    last_byte_mask = 0;

  const uint32_t row_size =
      (bitmap_info.bmiHeader.biWidth + kBitsPeruint32 - 1) / kBitsPeruint32;
  PixelData data(GetBitmapData(icon.hbmMask, bitmap_info, hdc));
  if (data == nullptr)
    return kDefaultHeight;

  // There are 2 types of cursors: Ones that cover the area underneath
  // completely (i.e. hand cursor) and ones that partially cover
  // and partially blend with background (i. e. I-beam cursor).
  // These will have either 1 square mask or 2 masks stacked on top
  // of each other (xor mask and and mask).
  const bool has_xor_mask =
      bitmap_info.bmiHeader.biHeight == 2 * bitmap_info.bmiHeader.biWidth;
  const int cursor_height =
      has_xor_mask ? static_cast<int>(bitmap_info.bmiHeader.biHeight / 2)
                   : static_cast<int>(bitmap_info.bmiHeader.biHeight);
  int xor_offset;
  if (has_xor_mask) {
    for (xor_offset = 0; xor_offset < cursor_height; ++xor_offset) {
      const uint32_t row_start = row_size * xor_offset;
      const uint32_t row_boundary = row_start + row_size;
      for (uint32_t i = row_start; i < row_boundary; ++i)
        data.get()[i] = ~(data.get()[i]);
      if (!IsRowTransparent(data, row_size, last_byte_mask, xor_offset)) {
        break;
      }
    }
  } else {
    xor_offset = cursor_height;
  }

  int and_offset;

  for (and_offset = has_xor_mask ? cursor_height : 0;
       and_offset < bitmap_info.bmiHeader.biHeight; ++and_offset) {
    if (!IsRowTransparent(data, row_size, last_byte_mask, and_offset)) {
      break;
    }
  }
  if (has_xor_mask) {
    and_offset -= cursor_height;
  }
  const int offset = std::min(xor_offset, and_offset);

  DeleteObject(icon.hbmColor);
  DeleteObject(icon.hbmMask);

  return cursor_height - offset - icon.yHotspot + 1;
}

}  // namespace

namespace views {
namespace corewm {

int GetCurrentCursorVisibleHeight() {
  CURSORINFO cursor = {0};
  cursor.cbSize = sizeof(cursor);
  GetCursorInfo(&cursor);

  if (cached_heights == nullptr)
    cached_heights = new HeightStorage;

  HeightStorage::const_iterator cached_height =
      cached_heights->find(cursor.hCursor);
  if (cached_height != cached_heights->end())
    return cached_height->second;

  const int height = CalculateCursorHeight(cursor.hCursor);
  (*cached_heights)[cursor.hCursor] = height;

  return height;
}

}  // namespace corewm
}  // namespace views
