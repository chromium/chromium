// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_GEOMETRY_SIZE_F_H_
#define UI_GFX_GEOMETRY_SIZE_F_H_

#include <iosfwd>
#include <string>

#include "base/gtest_prod_util.h"
#include "build/build_config.h"
#include "ui/gfx/geometry/geometry_export.h"
#include "ui/gfx/geometry/size.h"

#if BUILDFLAG(IS_APPLE)
struct CGSize;
#endif

namespace gfx {

FORWARD_DECLARE_TEST(SizeTest, TrivialDimensionTests);
FORWARD_DECLARE_TEST(SizeTest, ClampsToZero);
FORWARD_DECLARE_TEST(SizeTest, ConsistentClamping);

// A floating version of gfx::Size.
class GEOMETRY_EXPORT SizeF {
 public:
  constexpr SizeF() : width_(0.f), height_(0.f) {}
  constexpr SizeF(float width, float height)
      : width_(clamp(width)), height_(clamp(height)) {}

  constexpr explicit SizeF(const Size& size)
      : SizeF(static_cast<float>(size.width()),
              static_cast<float>(size.height())) {}

#if BUILDFLAG(IS_APPLE)
  explicit SizeF(const CGSize&);
  CGSize ToCGSize() const;
#endif

  constexpr float width() const { return width_; }
  constexpr float height() const { return height_; }

  void set_width(float width) { width_ = clamp(width); }
  void set_height(float height) { height_ = clamp(height); }

  void operator+=(const SizeF& size) {
    SetSize(width_ + size.width_, height_ + size.height_);
  }
  void operator-=(const SizeF& size) {
    SetSize(width_ - size.width_, height_ - size.height_);
  }

  float GetArea() const;

  float AspectRatio() const { return width_ / height_; }

  void SetSize(float width, float height) {
    set_width(width);
    set_height(height);
  }

  void Enlarge(float grow_width, float grow_height);

  void SetToMin(const SizeF& other);
  void SetToMax(const SizeF& other);

  // Expands width/height to the next representable value.
  void SetToNextWidth() { width_ = next(width_); }
  void SetToNextHeight() { height_ = next(height_); }

  constexpr bool IsEmpty() const { return !width() || !height(); }
  constexpr bool IsZero() const { return !width() && !height(); }

  void Scale(float scale) {
    Scale(scale, scale);
  }

  void Scale(float x_scale, float y_scale) {
    SetSize(width() * x_scale, height() * y_scale);
  }

  // Scales the size by the inverse of the given scale (by dividing).
  void InvScale(float inv_scale) { InvScale(inv_scale, inv_scale); }

  void InvScale(float inv_x_scale, float inv_y_scale) {
    width_ /= inv_x_scale;
    height_ /= inv_y_scale;
  }

  void Transpose() {
    using std::swap;
    swap(width_, height_);
  }

  std::string ToString() const;

 private:
  FRIEND_TEST_ALL_PREFIXES(SizeFTest, IsEmpty);
  FRIEND_TEST_ALL_PREFIXES(SizeFTest, ClampsToZero);
  FRIEND_TEST_ALL_PREFIXES(SizeFTest, ConsistentClamping);

  static constexpr float kTrivial = 8.f * std::numeric_limits<float>::epsilon();

  static constexpr float clamp(float f) { return f > kTrivial ? f : 0.f; }

  static float next(float f) {
    return std::nextafter(std::max(kTrivial, f),
                          std::numeric_limits<float>::max());
  }

  float width_;
  float height_;
};

constexpr bool operator==(const SizeF& lhs, const SizeF& rhs) {
  return lhs.width() == rhs.width() && lhs.height() == rhs.height();
}

constexpr bool operator!=(const SizeF& lhs, const SizeF& rhs) {
  return !(lhs == rhs);
}

inline SizeF operator+(const SizeF& lhs, const SizeF& rhs) {
  return SizeF(lhs.width() + rhs.width(), lhs.height() + rhs.height());
}

inline SizeF operator-(const SizeF& lhs, const SizeF& rhs) {
  return SizeF(lhs.width() - rhs.width(), lhs.height() - rhs.height());
}

GEOMETRY_EXPORT SizeF ScaleSize(const SizeF& p, float x_scale, float y_scale);

inline SizeF ScaleSize(const SizeF& p, float scale) {
  return ScaleSize(p, scale, scale);
}

inline SizeF TransposeSize(const SizeF& s) {
  return SizeF(s.height(), s.width());
}

// This is declared here for use in gtest-based unit tests but is defined in
// the //ui/gfx:test_support target. Depend on that to use this in your unit
// test. This should not be used in production code - call ToString() instead.
void PrintTo(const SizeF& size, ::std::ostream* os);

}  // namespace gfx

#endif  // UI_GFX_GEOMETRY_SIZE_F_H_
