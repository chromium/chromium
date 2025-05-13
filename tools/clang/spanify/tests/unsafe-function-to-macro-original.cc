// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <tuple>

#include "base/memory/raw_ptr.h"
#include "third_party/do_not_rewrite/third_party_api.h"

// ---- Test cases of C++ member function calls ----------------------------

SkBitmap* GetSkBitmap();

void test_no_arg() {
  SkBitmap sk_bitmap;
  // Expected rewrite:
  // base::span<uint32_t> image_row =
  //     UNSAFE_SKBITMAP_NOARGFORTESTING(sk_bitmap);
  uint32_t* image_row = sk_bitmap.NoArgForTesting();  // IN-TEST
  std::ignore = image_row[0];
}

void test_with_args() {
  const SkBitmap sk_bitmap;
  // Expected rewrite:
  // base::span<uint32_t> image_row =
  //     UNSAFE_SKBITMAP_GETADDR32(sk_bitmap, 1, 2);
  uint32_t* image_row = sk_bitmap.getAddr32(1, 2);
  std::ignore = image_row[0];
}

void test_receiver_is_expression() {
  // Expected rewrite:
  // base::span<uint32_t> image_row =
  //     UNSAFE_SKBITMAP_GETADDR32(GetSkBitmap(), 1, 2);
  uint32_t* image_row = GetSkBitmap()->getAddr32(1, 2);
  std::ignore = image_row[0];
}

void test_receiver_is_pointer() {
  const SkBitmap* sk_bitmap = GetSkBitmap();
  // Expected rewrite:
  // base::span<uint32_t> image_row =
  //     UNSAFE_SKBITMAP_GETADDR32(sk_bitmap, 1, 2);
  uint32_t* image_row = sk_bitmap->getAddr32(1, 2);
  std::ignore = image_row[0];
}

void test_receiver_is_reference() {
  const SkBitmap& sk_bitmap = *GetSkBitmap();
  // Expected rewrite:
  // base::span<uint32_t> image_row =
  //     UNSAFE_SKBITMAP_GETADDR32(sk_bitmap, 1, 2);
  uint32_t* image_row = sk_bitmap.getAddr32(1, 2);
  std::ignore = image_row[0];
}

void test_receiver_is_std_unique_ptr() {
  std::unique_ptr<SkBitmap> sk_bitmap = std::make_unique<SkBitmap>();
  // Expected rewrite:
  // base::span<uint32_t> image_row =
  //     UNSAFE_SKBITMAP_GETADDR32(sk_bitmap, 1, 2);
  uint32_t* image_row = sk_bitmap->getAddr32(1, 2);
  std::ignore = image_row[0];
}

void test_receiver_is_base_raw_ptr() {
  base::raw_ptr<SkBitmap> sk_bitmap = GetSkBitmap();
  // Expected rewrite:
  // base::span<uint32_t> image_row =
  //     UNSAFE_SKBITMAP_GETADDR32(sk_bitmap, 1, 2);
  uint32_t* image_row = sk_bitmap->getAddr32(1, 2);
  std::ignore = image_row[0];
}

// ---- Test cases of C/C++ free function calls ----------------------------

void test_hb_buffer_get_glyph_positions() {
  struct hb_buffer_t buffer;
  unsigned int length;
  // Expected rewrite:
  // base::span<struct hb_glyph_position_t> positions =
  //     UNSAFE_HB_BUFFER_GET_GLYPH_POSITIONS(&buffer, &length);
  struct hb_glyph_position_t* positions =
      hb_buffer_get_glyph_positions(&buffer, &length);
  std::ignore = positions[0];
}
