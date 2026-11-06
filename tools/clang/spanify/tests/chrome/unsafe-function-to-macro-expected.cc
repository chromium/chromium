// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <tuple>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/numerics/safe_conversions.h"
#include "third_party/do_not_rewrite/third_party_api.h"

#define UNSAFE_BUFFERS(...) __VA_ARGS__
#define UNSAFE_TODO(...) UNSAFE_BUFFERS(__VA_ARGS__)

// ---- Test cases of C++ member function calls ----------------------------

SkBitmap* GetSkBitmap();
SkBitmap& GetSkBitmapAt(int i);

void test_no_arg() {
  SkBitmap sk_bitmap;
  // Expected rewrite:
  // base::span<uint32_t> image_row = UNSAFE_TODO(base::span(
  //     sk_bitmap.NoArgForTesting(),
  //     base::checked_cast<size_t>(sk_bitmap.width())));
  base::span<uint32_t> image_row = UNSAFE_TODO(
      base::span(sk_bitmap.NoArgForTesting(),
                 base::checked_cast<size_t>(sk_bitmap.width())));  // IN-TEST
  std::ignore = image_row[0];
}

void test_with_args() {
  const SkBitmap sk_bitmap;
  // Expected rewrite:
  // base::span<uint32_t> image_row = UNSAFE_TODO(base::span(
  //     sk_bitmap.getAddr32(1, 2),
  //     base::checked_cast<size_t>(sk_bitmap.width() - 1)));
  base::span<uint32_t> image_row = UNSAFE_TODO(
      base::span(sk_bitmap.getAddr32(1, 2),
                 base::checked_cast<size_t>(sk_bitmap.width() - 1)));
  std::ignore = image_row[0];
}

void test_receiver_is_expression() {
  // The receiver is a call, which would run twice.
  // No rewrite expected.
  uint32_t* image_row = GetSkBitmap()->getAddr32(1, 2);
  std::ignore = image_row[0];
}

void test_receiver_is_pointer() {
  const SkBitmap* sk_bitmap = GetSkBitmap();
  // Expected rewrite:
  // base::span<uint32_t> image_row = UNSAFE_TODO(base::span(
  //     sk_bitmap->getAddr32(1, 2),
  //     base::checked_cast<size_t>(sk_bitmap->width() - 1)));
  base::span<uint32_t> image_row = UNSAFE_TODO(
      base::span(sk_bitmap->getAddr32(1, 2),
                 base::checked_cast<size_t>(sk_bitmap->width() - 1)));
  std::ignore = image_row[0];
}

void test_receiver_is_reference() {
  const SkBitmap& sk_bitmap = *GetSkBitmap();
  // Expected rewrite:
  // base::span<uint32_t> image_row = UNSAFE_TODO(base::span(
  //     sk_bitmap.getAddr32(1, 2),
  //     base::checked_cast<size_t>(sk_bitmap.width() - 1)));
  base::span<uint32_t> image_row = UNSAFE_TODO(
      base::span(sk_bitmap.getAddr32(1, 2),
                 base::checked_cast<size_t>(sk_bitmap.width() - 1)));
  std::ignore = image_row[0];
}

void test_receiver_is_std_unique_ptr() {
  std::unique_ptr<SkBitmap> sk_bitmap = std::make_unique<SkBitmap>();
  // Expected rewrite:
  // base::span<uint32_t> image_row = UNSAFE_TODO(base::span(
  //     sk_bitmap->getAddr32(1, 2),
  //     base::checked_cast<size_t>(sk_bitmap->width() - 1)));
  base::span<uint32_t> image_row = UNSAFE_TODO(
      base::span(sk_bitmap->getAddr32(1, 2),
                 base::checked_cast<size_t>(sk_bitmap->width() - 1)));
  std::ignore = image_row[0];
}

void test_receiver_is_base_raw_ptr() {
  base::raw_ptr<SkBitmap> sk_bitmap = GetSkBitmap();
  // Expected rewrite:
  // base::span<uint32_t> image_row = UNSAFE_TODO(base::span(
  //     sk_bitmap->getAddr32(1, 2),
  //     base::checked_cast<size_t>(sk_bitmap->width() - 1)));
  base::span<uint32_t> image_row = UNSAFE_TODO(
      base::span(sk_bitmap->getAddr32(1, 2),
                 base::checked_cast<size_t>(sk_bitmap->width() - 1)));
  std::ignore = image_row[0];
}

class MyBitmap : public SkBitmap {
 public:
  void test_receiver_is_implicit_this() {
    // Expected rewrite:
    // base::span<uint32_t> image_row = UNSAFE_TODO(base::span(
    //     getAddr32(1, 2), base::checked_cast<size_t>(width() - 1)));
    base::span<uint32_t> image_row = UNSAFE_TODO(
        base::span(getAddr32(1, 2), base::checked_cast<size_t>(width() - 1)));
    std::ignore = image_row[0];
  }
};

void test_already_in_unsafe_todo(const SkBitmap& sk_bitmap) {
  // The existing UNSAFE_TODO() covers the span construction.
  // Expected rewrite:
  // base::span<uint32_t> image_row = UNSAFE_TODO(base::span(
  //     sk_bitmap.getAddr32(1, 2),
  //     base::checked_cast<size_t>(sk_bitmap.width() - 1)));
  base::span<uint32_t> image_row = UNSAFE_TODO(
      base::span(sk_bitmap.getAddr32(1, 2),
                 base::checked_cast<size_t>(sk_bitmap.width() - 1)));
  std::ignore = image_row[0];
}

void test_arg_with_side_effects(const SkBitmap& sk_bitmap, int x) {
  // The first argument would be evaluated twice.
  // No rewrite expected.
  uint32_t* image_row = sk_bitmap.getAddr32(x++, 2);
  std::ignore = image_row[0];
}

void test_volatile_arg(const SkBitmap& sk_bitmap) {
  volatile int x = 1;
  // Reading `x` again may give another value.
  // No rewrite expected.
  uint32_t* image_row = sk_bitmap.getAddr32(x, 2);
  std::ignore = image_row[0];
}

struct VolatileColumn {
  volatile int x;
};

void test_volatile_member_arg(const SkBitmap& sk_bitmap,
                              const VolatileColumn& column) {
  // Reading `column.x` again may give another value.
  // No rewrite expected.
  uint32_t* image_row = sk_bitmap.getAddr32(column.x, 2);
  std::ignore = image_row[0];
}

extern const SkBitmap* g_sk_bitmap;
#define G_SK_BITMAP (*g_sk_bitmap)

void test_receiver_from_macro() {
  // The call starts in a macro body.
  // No rewrite expected.
  uint32_t* image_row = G_SK_BITMAP.getAddr32(1, 2);
  std::ignore = image_row[0];
}

void test_compound_arg(const SkBitmap& sk_bitmap, int x) {
  // Expected rewrite:
  // base::span<uint32_t> image_row = UNSAFE_TODO(base::span(
  //     sk_bitmap.getAddr32(x + 1, 2),
  //     base::checked_cast<size_t>(sk_bitmap.width() - (x + 1))));
  base::span<uint32_t> image_row = UNSAFE_TODO(
      base::span(sk_bitmap.getAddr32(x + 1, 2),
                 base::checked_cast<size_t>(sk_bitmap.width() - (x + 1))));
  std::ignore = image_row[0];
}

struct Point {
  int x() const;
};

void test_arg_is_const_getter(const SkBitmap& sk_bitmap, const Point& point) {
  // Expected rewrite:
  // base::span<uint32_t> image_row = UNSAFE_TODO(base::span(
  //     sk_bitmap.getAddr32(point.x(), 2),
  //     base::checked_cast<size_t>(sk_bitmap.width() - point.x())));
  base::span<uint32_t> image_row = UNSAFE_TODO(
      base::span(sk_bitmap.getAddr32(point.x(), 2),
                 base::checked_cast<size_t>(sk_bitmap.width() - point.x())));
  std::ignore = image_row[0];
}

void test_unsigned_arg(const SkBitmap& sk_bitmap, size_t x) {
  // `width() - x` would be unsigned and wrap around.
  // No rewrite expected.
  uint32_t* image_row = sk_bitmap.getAddr32(x, 2);
  std::ignore = image_row[0];
}

void test_receiver_is_call_with_args(int i) {
  // The receiver is a call, which would run twice.
  // No rewrite expected.
  uint32_t* image_row = GetSkBitmapAt(i).getAddr32(1, 2);
  std::ignore = image_row[0];
}

// ---- Test cases of C/C++ free function calls ----------------------------

void test_hb_buffer_get_glyph_positions() {
  struct hb_buffer_t buffer;
  unsigned int length;
  // Expected rewrite:
  // base::span<struct hb_glyph_position_t> positions = UNSAFE_TODO(base::span(
  //     hb_buffer_get_glyph_positions(&buffer, &length),
  //     hb_buffer_get_length(&buffer)));
  base::span<struct hb_glyph_position_t> positions =
      UNSAFE_TODO(base::span(hb_buffer_get_glyph_positions(&buffer, &length),
                             hb_buffer_get_length(&buffer)));
  std::ignore = positions[0];
}

void test_hb_buffer_get_glyph_infos(hb_buffer_t* buffer) {
  // Expected rewrite:
  // base::span<hb_glyph_info_t> infos = UNSAFE_TODO(base::span(
  //     hb_buffer_get_glyph_infos(buffer, nullptr),
  //     hb_buffer_get_length(buffer)));
  base::span<hb_glyph_info_t> infos =
      UNSAFE_TODO(base::span(hb_buffer_get_glyph_infos(buffer, nullptr),
                             hb_buffer_get_length(buffer)));
  std::ignore = infos[0];
}

void test_crypto_buffer_data(const CRYPTO_BUFFER* buffer) {
  // Expected rewrite:
  // base::span<const uint8_t> data = UNSAFE_TODO(
  //     base::span(CRYPTO_BUFFER_data(buffer), CRYPTO_BUFFER_len(buffer)));
  base::span<const uint8_t> data = UNSAFE_TODO(
      base::span(CRYPTO_BUFFER_data(buffer), CRYPTO_BUFFER_len(buffer)));
  std::ignore = data[0];
}
