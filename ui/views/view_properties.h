// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_VIEW_PROPERTIES_H_
#define UI_VIEWS_VIEW_PROPERTIES_H_

#include "ui/base/class_property.h"
#include "ui/views/views_export.h"

class SkPath;

namespace gfx {
class Insets;
}  // namespace gfx

namespace views {

class BubbleDialogDelegateView;

// The hit test component (e.g. HTCLIENT) for a View in a window frame. Defaults
// to HTNOWHERE.
VIEWS_EXPORT extern const ui::ClassProperty<int>* const kHitTestComponentKey;

// A property to store margins around the outer perimeter of the view. Margins
// are outside the bounds of the view. This is used by various layout managers
// to position views with the proper spacing between them.
VIEWS_EXPORT extern const ui::ClassProperty<gfx::Insets*>* const kMarginsKey;

// A property to store the bubble dialog anchored to this view, to
// enable the bubble's contents to be included in the focus order.
VIEWS_EXPORT extern const ui::ClassProperty<BubbleDialogDelegateView*>* const
    kAnchoredDialogKey;

// A property to store a highlight path related to the view. This is nominally
// used by the default inkdrop and focus ring that are both used to highlight
// the view in different ways.
VIEWS_EXPORT extern const ui::ClassProperty<SkPath*>* const kHighlightPathKey;

}  // namespace views

// Declaring the template specialization here to make sure that the
// compiler in all builds, including jumbo builds, always knows about
// the specialization before the first template instance use. Using a
// template instance before its specialization is declared in a
// translation unit is a C++ error.
DECLARE_EXPORTED_UI_CLASS_PROPERTY_TYPE(VIEWS_EXPORT, gfx::Insets*);
DECLARE_EXPORTED_UI_CLASS_PROPERTY_TYPE(VIEWS_EXPORT,
                                        views::BubbleDialogDelegateView*);
DECLARE_EXPORTED_UI_CLASS_PROPERTY_TYPE(VIEWS_EXPORT, SkPath*);
#endif  // UI_VIEWS_VIEW_PROPERTIES_H_
