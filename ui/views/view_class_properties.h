// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_VIEW_CLASS_PROPERTIES_H_
#define UI_VIEWS_VIEW_CLASS_PROPERTIES_H_

#include "ui/base/class_property.h"
#include "ui/views/views_export.h"

namespace gfx {
class Insets;
}  // namespace gfx

namespace views {

class BubbleDialogDelegateView;
class FlexSpecification;
class HighlightPathGenerator;

// The hit test component (e.g. HTCLIENT) for a View in a window frame. Defaults
// to HTNOWHERE.
VIEWS_EXPORT extern const ui::ClassProperty<int>* const kHitTestComponentKey;

// A property to store margins around the outer perimeter of the view. Margins
// are outside the bounds of the view. This is used by various layout managers
// to position views with the proper spacing between them.
//
// Used by multiple layout managers.
VIEWS_EXPORT extern const ui::ClassProperty<gfx::Insets*>* const kMarginsKey;

// A property to store the internal padding contained in a view. When doing
// layout, this padding is counted against the required margin around the view,
// effectively reducing the size of the margin (to a minimum of zero). Examples
// include expansion of buttons in touch mode and empty areas that serve as
// resize handles.
//
// Used by FlexLayout.
VIEWS_EXPORT extern const ui::ClassProperty<gfx::Insets*>* const
    kInternalPaddingKey;

// A property to store the bubble dialog anchored to this view, to
// enable the bubble's contents to be included in the focus order.
VIEWS_EXPORT extern const ui::ClassProperty<BubbleDialogDelegateView*>* const
    kAnchoredDialogKey;

// A property to store a highlight-path generator. This generator is used to
// generate a highlight path for focus rings or ink-drop effects.
VIEWS_EXPORT extern const ui::ClassProperty<HighlightPathGenerator*>* const
    kHighlightPathGeneratorKey;

// A property to store how a view should flex when placed in a layout.
// Currently only supported by FlexLayout.
VIEWS_EXPORT extern const ui::ClassProperty<FlexSpecification*>* const
    kFlexBehaviorKey;

}  // namespace views

// Declaring the template specialization here to make sure that the
// compiler in all builds, including jumbo builds, always knows about
// the specialization before the first template instance use. Using a
// template instance before its specialization is declared in a
// translation unit is a C++ error.
DECLARE_EXPORTED_UI_CLASS_PROPERTY_TYPE(VIEWS_EXPORT, gfx::Insets*)
DECLARE_EXPORTED_UI_CLASS_PROPERTY_TYPE(VIEWS_EXPORT,
                                        views::BubbleDialogDelegateView*)
DECLARE_EXPORTED_UI_CLASS_PROPERTY_TYPE(VIEWS_EXPORT,
                                        views::HighlightPathGenerator*)
DECLARE_EXPORTED_UI_CLASS_PROPERTY_TYPE(VIEWS_EXPORT, views::FlexSpecification*)
#endif  // UI_VIEWS_VIEW_CLASS_PROPERTIES_H_
