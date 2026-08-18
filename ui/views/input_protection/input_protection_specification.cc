// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/input_protection/input_protection_specification.h"

#include <utility>

#include "ui/views/view.h"

DEFINE_EXPORTED_UI_CLASS_PROPERTY_TYPE(VIEWS_EXPORT,
                                       views::InputProtectionSpecification*)

namespace views {

DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(views::InputProtectionSpecification,
                                   kInputProtectionKey)

InputProtectionSpecification::InputProtectionSpecification(
    GetBoundsCallback<View> callback)
    : callback_(std::move(callback)) {}

InputProtectionSpecification::~InputProtectionSpecification() = default;

std::vector<gfx::Rect> InputProtectionSpecification::GetProtectedBoundsInScreen(
    const View& view) const {
  std::vector<gfx::Rect> local_bounds = callback_.Run(&view);
  std::vector<gfx::Rect> screen_bounds;

  // Retrieve the view-defined protected bounds (which are in local coordinates
  // of `view`), clip them to the `view` actual boundaries for safety, and
  // convert the remaining valid bounds to screen coordinates.
  const gfx::Rect local_bounds_limit = view.GetLocalBounds();
  for (auto protected_rect : local_bounds) {
    protected_rect.Intersect(local_bounds_limit);
    if (!protected_rect.IsEmpty()) {
      View::ConvertRectToScreen(&view, &protected_rect);
      screen_bounds.push_back(protected_rect);
    }
  }

  return screen_bounds;
}

}  // namespace views
