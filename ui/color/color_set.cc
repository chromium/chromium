// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/color/color_set.h"

namespace ui {

ColorSet::ColorSet(ColorSetId id, ColorMap&& colors)
    : id(id), colors(std::move(colors)) {}

ColorSet::ColorSet(ColorSet&&) noexcept = default;

ColorSet& ColorSet::operator=(ColorSet&&) noexcept = default;

ColorSet::~ColorSet() = default;

}  // namespace ui
