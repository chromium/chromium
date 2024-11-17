// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_location_and_scroll_updates.h"

#include "ui/accessibility/ax_relative_bounds.h"

namespace ui {
AXLocationChange::AXLocationChange(int id, AXRelativeBounds& bounds)
    : id(id), new_location(bounds) {}
AXLocationChange::AXLocationChange(AXLocationChange&& other) = default;
AXLocationChange& AXLocationChange::operator=(AXLocationChange&& other) =
    default;
AXLocationChange::AXLocationChange(const AXLocationChange& other) = default;
AXLocationChange& AXLocationChange::operator=(const AXLocationChange& other) =
    default;
AXLocationChange::~AXLocationChange() = default;

AXScrollChange::AXScrollChange(int id, int x, int y)
    : id(id), scroll_x(x), scroll_y(y) {}
AXScrollChange::AXScrollChange(AXScrollChange&& other) = default;
AXScrollChange& AXScrollChange::operator=(AXScrollChange&& other) = default;
AXScrollChange::AXScrollChange(const AXScrollChange& other) = default;
AXScrollChange& AXScrollChange::operator=(const AXScrollChange& other) =
    default;
AXScrollChange::~AXScrollChange() = default;

AXLocationAndScrollUpdates::AXLocationAndScrollUpdates() = default;
AXLocationAndScrollUpdates::AXLocationAndScrollUpdates(
    AXLocationAndScrollUpdates&& other) = default;
AXLocationAndScrollUpdates& AXLocationAndScrollUpdates::operator=(
    AXLocationAndScrollUpdates&& other) = default;
AXLocationAndScrollUpdates::~AXLocationAndScrollUpdates() = default;
}  // namespace ui
