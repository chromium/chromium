// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/color/dynamic_color/palette.h"

#include "base/check_op.h"
#include "third_party/material_color_utilities/src/cpp/cam/cam.h"

namespace ui {

namespace {

using material_color_utilities::Argb;
using material_color_utilities::Cam;
using material_color_utilities::CamFromInt;
using material_color_utilities::IntFromHcl;

}  // namespace

TonalPalette::TonalPalette(SkColor argb) {
  Cam cam = CamFromInt(static_cast<Argb>(argb));
  hue_ = cam.hue;
  chroma_ = cam.chroma;
}

TonalPalette::TonalPalette(double hue, double chroma)
    : hue_(hue), chroma_(chroma) {}

TonalPalette::TonalPalette(const TonalPalette& other) = default;
TonalPalette& TonalPalette::operator=(const TonalPalette&) = default;

TonalPalette::~TonalPalette() = default;

SkColor TonalPalette::get(float tone) const {
  CHECK_LE(tone, 100.0f);
  CHECK_GE(tone, 0.0f);
  return static_cast<SkColor>(IntFromHcl(hue_, chroma_, tone));
}

}  // namespace ui
