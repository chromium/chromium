// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/geometry/size_f.h"

#include "base/strings/stringprintf.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_IOS)
#include <CoreGraphics/CoreGraphics.h>
#elif BUILDFLAG(IS_MAC)
#include <ApplicationServices/ApplicationServices.h>
#endif

namespace gfx {

#if BUILDFLAG(IS_APPLE)
SizeF::SizeF(const CGSize& size) : SizeF(size.width, size.height) {}

CGSize SizeF::ToCGSize() const {
  return CGSizeMake(width(), height());
}
#endif

float SizeF::GetArea() const {
  return width() * height();
}

void SizeF::Enlarge(float grow_width, float grow_height) {
  SetSize(width() + grow_width, height() + grow_height);
}

void SizeF::SetToMin(const SizeF& other) {
  width_ = std::min(width_, other.width_);
  height_ = std::min(height_, other.height_);
}

void SizeF::SetToMax(const SizeF& other) {
  width_ = std::max(width_, other.width_);
  height_ = std::max(height_, other.height_);
}

std::string SizeF::ToString() const {
  return base::StringPrintf("%gx%g", width(), height());
}

SizeF ScaleSize(const SizeF& s, float x_scale, float y_scale) {
  SizeF scaled_s(s);
  scaled_s.Scale(x_scale, y_scale);
  return scaled_s;
}

}  // namespace gfx
