// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_AUTOCORRECT_INFO_H_
#define UI_BASE_IME_AUTOCORRECT_INFO_H_

#include "base/component_export.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/range/range.h"

namespace ui {

// Represents autocorrection.
struct COMPONENT_EXPORT(UI_BASE_IME_TYPES) AutocorrectInfo {
  // Range of the autocorrection.
  gfx::Range range;

  // Bounding box of the characters.
  gfx::Rect bounds;
};

}  // namespace ui

#endif  // UI_BASE_IME_AUTOCORRECT_INFO_H_
