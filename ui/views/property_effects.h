// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_PROPERTY_EFFECTS_H_
#define UI_VIEWS_PROPERTY_EFFECTS_H_

namespace views {

// The elements in PropertyEffects define what effect(s) a changed Property has
// on the containing class.
enum class PropertyEffects {
  kNone,
  // Any changes to the property should cause the container to invalidate the
  // current layout state.
  kLayout,
  // Changes to the property should cause the container to schedule a painting
  // update.
  kPaint,
  // Changes to the property should cause the preferred size to change. This
  // implies kLayout.
  kPreferredSizeChanged,
};

}  // namespace views

#endif  // UI_VIEWS_PROPERTY_EFFECTS_H_
