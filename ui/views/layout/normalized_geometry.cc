// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/layout/normalized_geometry.h"

#include <algorithm>
#include <tuple>
#include <utility>

#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/layout/flex_layout_types.h"

namespace views {

// NormalizedPoint -------------------------------------------------------------

void NormalizedPoint::SetPoint(int main, int cross) {
  main_ = main;
  cross_ = cross;
}

void NormalizedPoint::Offset(int delta_main, int delta_cross) {
  main_ += delta_main;
  cross_ += delta_cross;
}

bool NormalizedPoint::operator==(const NormalizedPoint& other) const {
  return std::tie(main_, cross_) == std::tie(other.main_, other.cross_);
}

bool NormalizedPoint::operator!=(const NormalizedPoint& other) const {
  return !(*this == other);
}

bool NormalizedPoint::operator<(const NormalizedPoint& other) const {
  return std::tie(main_, cross_) < std::tie(other.main_, other.cross_);
}

std::string NormalizedPoint::ToString() const {
  return base::StringPrintf("%d, %d", main(), cross());
}

// NormalizedSize --------------------------------------------------------------

void NormalizedSize::SetSize(int main, int cross) {
  main_ = std::max(0, main);
  cross_ = std::max(0, cross);
}

void NormalizedSize::Enlarge(int delta_main, int delta_cross) {
  main_ = std::max(0, main_ + delta_main);
  cross_ = std::max(0, cross_ + delta_cross);
}

void NormalizedSize::SetToMax(int main, int cross) {
  main_ = std::max(main_, main);
  cross_ = std::max(cross_, cross);
}

void NormalizedSize::SetToMin(int main, int cross) {
  main_ = std::clamp(main, 0, main_);
  cross_ = std::clamp(cross, 0, cross_);
}

void NormalizedSize::SetToMax(const NormalizedSize& other) {
  SetToMax(other.main(), other.cross());
}

void NormalizedSize::SetToMin(const NormalizedSize& other) {
  SetToMin(other.main(), other.cross());
}

bool NormalizedSize::operator==(const NormalizedSize& other) const {
  return std::tie(main_, cross_) == std::tie(other.main_, other.cross_);
}

bool NormalizedSize::operator!=(const NormalizedSize& other) const {
  return !(*this == other);
}

bool NormalizedSize::operator<(const NormalizedSize& other) const {
  return std::tie(main_, cross_) < std::tie(other.main_, other.cross_);
}

std::string NormalizedSize::ToString() const {
  return base::StringPrintf("%d x %d", main(), cross());
}

// NormalizedInsets ------------------------------------------------------------

bool NormalizedInsets::operator==(const NormalizedInsets& other) const {
  return std::tie(main_, cross_) == std::tie(other.main_, other.cross_);
}

bool NormalizedInsets::operator!=(const NormalizedInsets& other) const {
  return !(*this == other);
}

bool NormalizedInsets::operator<(const NormalizedInsets& other) const {
  return std::tie(main_, cross_) < std::tie(other.main_, other.cross_);
}

std::string NormalizedInsets::ToString() const {
  return base::StrCat(
      {"main: [", main_.ToString(), "], cross: [", cross_.ToString(), "]"});
}

// NormalizedSizeBounds --------------------------------------------------------

NormalizedSizeBounds::NormalizedSizeBounds() = default;

NormalizedSizeBounds::NormalizedSizeBounds(SizeBound main, SizeBound cross)
    : main_(std::move(main)), cross_(std::move(cross)) {}

NormalizedSizeBounds::NormalizedSizeBounds(const NormalizedSizeBounds& other)
    : main_(other.main()), cross_(other.cross()) {}

NormalizedSizeBounds::NormalizedSizeBounds(const NormalizedSize& other)
    : main_(other.main()), cross_(other.cross()) {}

void NormalizedSizeBounds::Expand(int main, int cross) {
  main_ = std::max<SizeBound>(0, main_ + main);
  cross_ = std::max<SizeBound>(0, cross_ + cross);
}

void NormalizedSizeBounds::Inset(const NormalizedInsets& insets) {
  Expand(-insets.main_size(), -insets.cross_size());
}

bool NormalizedSizeBounds::operator==(const NormalizedSizeBounds& other) const {
  return std::tie(main_, cross_) == std::tie(other.main_, other.cross_);
}

bool NormalizedSizeBounds::operator!=(const NormalizedSizeBounds& other) const {
  return !(*this == other);
}

bool NormalizedSizeBounds::operator<(const NormalizedSizeBounds& other) const {
  return std::tie(main_, cross_) < std::tie(other.main_, other.cross_);
}

std::string NormalizedSizeBounds::ToString() const {
  return base::StrCat({main_.ToString(), " x ", cross_.ToString()});
}

// NormalizedRect --------------------------------------------------------------

Span NormalizedRect::GetMainSpan() const {
  return Span(origin_main(), size_main());
}

void NormalizedRect::SetMainSpan(const Span& span) {
  set_origin_main(span.start());
  set_size_main(span.length());
}

void NormalizedRect::AlignMain(const Span& container,
                               LayoutAlignment alignment,
                               const Inset1D& margins) {
  Span temp = GetMainSpan();
  temp.Align(container, alignment, margins);
  SetMainSpan(temp);
}

Span NormalizedRect::GetCrossSpan() const {
  return Span(origin_cross(), size_cross());
}

void NormalizedRect::SetCrossSpan(const Span& span) {
  set_origin_cross(span.start());
  set_size_cross(span.length());
}

void NormalizedRect::AlignCross(const Span& container,
                                LayoutAlignment alignment,
                                const Inset1D& margins) {
  Span temp = GetCrossSpan();
  temp.Align(container, alignment, margins);
  SetCrossSpan(temp);
}

void NormalizedRect::SetRect(int origin_main,
                             int origin_cross,
                             int size_main,
                             int size_cross) {
  origin_.SetPoint(origin_main, origin_cross);
  size_.SetSize(size_main, size_cross);
}

void NormalizedRect::SetByBounds(int origin_main,
                                 int origin_cross,
                                 int max_main,
                                 int max_cross) {
  origin_.SetPoint(origin_main, origin_cross);
  size_.SetSize(std::max(0, max_main - origin_main),
                std::max(0, max_cross - origin_cross));
}

void NormalizedRect::Inset(const NormalizedInsets& insets) {
  Inset(insets.main_leading(), insets.cross_leading(), insets.main_trailing(),
        insets.cross_trailing());
}

void NormalizedRect::Inset(int main, int cross) {
  Inset(main, cross, main, cross);
}

void NormalizedRect::Inset(int main_leading,
                           int cross_leading,
                           int main_trailing,
                           int cross_trailing) {
  origin_.Offset(main_leading, cross_leading);
  size_.Enlarge(-(main_leading + main_trailing),
                -(cross_leading + cross_trailing));
}

void NormalizedRect::Offset(int main, int cross) {
  origin_.Offset(main, cross);
}

bool NormalizedRect::operator==(const NormalizedRect& other) const {
  return std::tie(origin_, size_) == std::tie(other.origin_, other.size_);
}

bool NormalizedRect::operator!=(const NormalizedRect& other) const {
  return !(*this == other);
}

bool NormalizedRect::operator<(const NormalizedRect& other) const {
  return std::tie(origin_, size_) < std::tie(other.origin_, other.size_);
}

std::string NormalizedRect::ToString() const {
  return base::StrCat({"(", origin_.ToString(), ") [", size_.ToString(), "]"});
}

// Normalization and Denormalization -------------------------------------------

NormalizedPoint Normalize(LayoutOrientation orientation,
                          const gfx::Point& point) {
  switch (orientation) {
    case LayoutOrientation::kHorizontal:
      return NormalizedPoint(point.x(), point.y());
    case LayoutOrientation::kVertical:
      return NormalizedPoint(point.y(), point.x());
  }
}

gfx::Point Denormalize(LayoutOrientation orientation,
                       const NormalizedPoint& point) {
  switch (orientation) {
    case LayoutOrientation::kHorizontal:
      return gfx::Point(point.main(), point.cross());
    case LayoutOrientation::kVertical:
      return gfx::Point(point.cross(), point.main());
  }
}

NormalizedSize Normalize(LayoutOrientation orientation, const gfx::Size& size) {
  switch (orientation) {
    case LayoutOrientation::kHorizontal:
      return NormalizedSize(size.width(), size.height());
    case LayoutOrientation::kVertical:
      return NormalizedSize(size.height(), size.width());
  }
}

gfx::Size Denormalize(LayoutOrientation orientation,
                      const NormalizedSize& size) {
  switch (orientation) {
    case LayoutOrientation::kHorizontal:
      return gfx::Size(size.main(), size.cross());
    case LayoutOrientation::kVertical:
      return gfx::Size(size.cross(), size.main());
  }
}

NormalizedSizeBounds Normalize(LayoutOrientation orientation,
                               const SizeBounds& bounds) {
  switch (orientation) {
    case LayoutOrientation::kHorizontal:
      return NormalizedSizeBounds(bounds.width(), bounds.height());
    case LayoutOrientation::kVertical:
      return NormalizedSizeBounds(bounds.height(), bounds.width());
  }
}

SizeBounds Denormalize(LayoutOrientation orientation,
                       const NormalizedSizeBounds& bounds) {
  switch (orientation) {
    case LayoutOrientation::kHorizontal:
      return SizeBounds(bounds.main(), bounds.cross());
    case LayoutOrientation::kVertical:
      return SizeBounds(bounds.cross(), bounds.main());
  }
}

NormalizedInsets Normalize(LayoutOrientation orientation,
                           const gfx::Insets& insets) {
  switch (orientation) {
    case LayoutOrientation::kHorizontal:
      return NormalizedInsets(insets.left(), insets.top(), insets.right(),
                              insets.bottom());
    case LayoutOrientation::kVertical:
      return NormalizedInsets(insets.top(), insets.left(), insets.bottom(),
                              insets.right());
  }
}

gfx::Insets Denormalize(LayoutOrientation orientation,
                        const NormalizedInsets& insets) {
  switch (orientation) {
    case LayoutOrientation::kHorizontal:
      return gfx::Insets::TLBR(insets.cross_leading(), insets.main_leading(),
                               insets.cross_trailing(), insets.main_trailing());
    case LayoutOrientation::kVertical:
      return gfx::Insets::TLBR(insets.main_leading(), insets.cross_leading(),
                               insets.main_trailing(), insets.cross_trailing());
  }
}

NormalizedRect Normalize(LayoutOrientation orientation,
                         const gfx::Rect& bounds) {
  return NormalizedRect(Normalize(orientation, bounds.origin()),
                        Normalize(orientation, bounds.size()));
}

gfx::Rect Denormalize(LayoutOrientation orientation,
                      const NormalizedRect& bounds) {
  return gfx::Rect(Denormalize(orientation, bounds.origin()),
                   Denormalize(orientation, bounds.size()));
}

int GetMainAxis(LayoutOrientation orientation, const gfx::Size& size) {
  switch (orientation) {
    case LayoutOrientation::kHorizontal:
      return size.width();
    case LayoutOrientation::kVertical:
      return size.height();
  }
}

int GetCrossAxis(LayoutOrientation orientation, const gfx::Size& size) {
  switch (orientation) {
    case LayoutOrientation::kHorizontal:
      return size.height();
    case LayoutOrientation::kVertical:
      return size.width();
  }
}

SizeBound GetMainAxis(LayoutOrientation orientation, const SizeBounds& size) {
  switch (orientation) {
    case LayoutOrientation::kHorizontal:
      return size.width();
    case LayoutOrientation::kVertical:
      return size.height();
  }
}

SizeBound GetCrossAxis(LayoutOrientation orientation, const SizeBounds& size) {
  switch (orientation) {
    case LayoutOrientation::kHorizontal:
      return size.height();
    case LayoutOrientation::kVertical:
      return size.width();
  }
}

void SetMainAxis(gfx::Size* size, LayoutOrientation orientation, int main) {
  switch (orientation) {
    case LayoutOrientation::kHorizontal:
      size->set_width(main);
      break;
    case LayoutOrientation::kVertical:
      size->set_height(main);
      break;
  }
}

void SetCrossAxis(gfx::Size* size, LayoutOrientation orientation, int cross) {
  switch (orientation) {
    case LayoutOrientation::kHorizontal:
      size->set_height(cross);
      break;
    case LayoutOrientation::kVertical:
      size->set_width(cross);
      break;
  }
}

void SetMainAxis(SizeBounds* size,
                 LayoutOrientation orientation,
                 SizeBound main) {
  switch (orientation) {
    case LayoutOrientation::kHorizontal:
      size->set_width(std::move(main));
      break;
    case LayoutOrientation::kVertical:
      size->set_height(std::move(main));
      break;
  }
}

void SetCrossAxis(SizeBounds* size,
                  LayoutOrientation orientation,
                  SizeBound cross) {
  switch (orientation) {
    case LayoutOrientation::kHorizontal:
      size->set_height(std::move(cross));
      break;
    case LayoutOrientation::kVertical:
      size->set_width(std::move(cross));
      break;
  }
}

}  // namespace views
