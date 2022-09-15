// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/geometry/size.h"

#include "base/numerics/clamped_math.h"
#include "base/numerics/safe_math.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "ui/gfx/geometry/size_conversions.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#elif BUILDFLAG(IS_IOS)
#include <CoreGraphics/CoreGraphics.h>
#elif BUILDFLAG(IS_MAC)
#include <ApplicationServices/ApplicationServices.h>
#endif

namespace gfx {

#if BUILDFLAG(IS_APPLE)
Size::Size(const CGSize& s) : Size(s.width, s.height) {}
#endif

void Size::operator+=(const Size& size) {
  Enlarge(size.width(), size.height());
}

void Size::operator-=(const Size& size) {
  Enlarge(-size.width(), -size.height());
}

#if BUILDFLAG(IS_WIN)
SIZE Size::ToSIZE() const {
  SIZE s;
  s.cx = width();
  s.cy = height();
  return s;
}
#elif BUILDFLAG(IS_APPLE)
CGSize Size::ToCGSize() const {
  return CGSizeMake(width(), height());
}
#endif

int Size::GetArea() const {
  return GetCheckedArea().ValueOrDie();
}

base::CheckedNumeric<int> Size::GetCheckedArea() const {
  base::CheckedNumeric<int> checked_area = width();
  checked_area *= height();
  return checked_area;
}

void Size::Enlarge(int grow_width, int grow_height) {
  SetSize(base::ClampAdd(width(), grow_width),
          base::ClampAdd(height(), grow_height));
}

void Size::SetToMin(const Size& other) {
  width_ = std::min(width_, other.width_);
  height_ = std::min(height_, other.height_);
}

void Size::SetToMax(const Size& other) {
  width_ = std::max(width_, other.width_);
  height_ = std::max(height_, other.height_);
}

std::string Size::ToString() const {
  return base::StringPrintf("%dx%d", width(), height());
}

Size ScaleToCeiledSize(const Size& size, float x_scale, float y_scale) {
  if (x_scale == 1.f && y_scale == 1.f)
    return size;
  return ToCeiledSize(ScaleSize(gfx::SizeF(size), x_scale, y_scale));
}

Size ScaleToCeiledSize(const Size& size, float scale) {
  if (scale == 1.f)
    return size;
  return ToCeiledSize(ScaleSize(gfx::SizeF(size), scale, scale));
}

Size ScaleToFlooredSize(const Size& size, float x_scale, float y_scale) {
  if (x_scale == 1.f && y_scale == 1.f)
    return size;
  return ToFlooredSize(ScaleSize(gfx::SizeF(size), x_scale, y_scale));
}

Size ScaleToFlooredSize(const Size& size, float scale) {
  if (scale == 1.f)
    return size;
  return ToFlooredSize(ScaleSize(gfx::SizeF(size), scale, scale));
}

Size ScaleToRoundedSize(const Size& size, float x_scale, float y_scale) {
  if (x_scale == 1.f && y_scale == 1.f)
    return size;
  return ToRoundedSize(ScaleSize(gfx::SizeF(size), x_scale, y_scale));
}

Size ScaleToRoundedSize(const Size& size, float scale) {
  if (scale == 1.f)
    return size;
  return ToRoundedSize(ScaleSize(gfx::SizeF(size), scale, scale));
}

}  // namespace gfx
