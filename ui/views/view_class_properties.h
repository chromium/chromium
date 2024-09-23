// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_VIEW_CLASS_PROPERTIES_H_
#define UI_VIEWS_VIEW_CLASS_PROPERTIES_H_

#include "ui/base/class_property.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/views_export.h"

namespace gfx {
class Insets;
}  // namespace gfx

namespace views {

class BoxLayoutFlexSpecification;
class DialogDelegate;
class FlexSpecification;
class HighlightPathGenerator;
class Widget;

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
VIEWS_EXPORT extern const ui::ClassProperty<DialogDelegate*>* const
    kAnchoredDialogKey;

// A property to store the anchor widget used for anchoring a bubble dialog
// to this view. If unset, the anchor widget is this view's containing widget.
//
// This is useful in macOS fullscreen where a sub views tree is moved to a
// separate overlay widget that has a higher z-order level. We anchor the bubble
// to the overlay widget to prevent the bubble from being occluded.
VIEWS_EXPORT extern const ui::ClassProperty<Widget*>* const
    kWidgetForAnchoringKey;

// A property to store how a view should flex when placed in a layout.
// Only supported by BoxLayout.
VIEWS_EXPORT extern const ui::ClassProperty<BoxLayoutFlexSpecification*>* const
    kBoxLayoutFlexKey;

// A property to store a highlight-path generator. This generator is used to
// generate a highlight path for focus rings or ink-drop effects.
VIEWS_EXPORT extern const ui::ClassProperty<HighlightPathGenerator*>* const
    kHighlightPathGeneratorKey;

// A property to store how a view should flex when placed in a layout.
// Currently only supported by FlexLayout.
VIEWS_EXPORT extern const ui::ClassProperty<FlexSpecification*>* const
    kFlexBehaviorKey;

VIEWS_EXPORT extern const ui::ClassProperty<LayoutAlignment*>* const
    kCrossAxisAlignmentKey;

// TableLayout-specific properties:
// Note that col/row span counts padding columns, so if you want to span a
// region consisting of <column><padding column><column>, it's a column span of
// 3, not 2.
VIEWS_EXPORT extern const ui::ClassProperty<gfx::Size*>* const
    kTableColAndRowSpanKey;
VIEWS_EXPORT extern const ui::ClassProperty<LayoutAlignment*>* const
    kTableHorizAlignKey;
VIEWS_EXPORT extern const ui::ClassProperty<LayoutAlignment*>* const
    kTableVertAlignKey;

// Property indicating whether a view should be ignored by a layout. Supported
// by View::DefaultFillLayout, BoxLayout, and all LayoutManagerBase-derived
// layouts including FlexLayout.
// TODO(kylixrd): Use for other layouts.
VIEWS_EXPORT extern const ui::ClassProperty<bool>* const
    kViewIgnoredByLayoutKey;

// Tag for the view associated with ui::ElementTracker.
VIEWS_EXPORT extern const ui::ClassProperty<ui::ElementIdentifier>* const
    kElementIdentifierKey;

}  // namespace views

// Declaring the template specialization here to make sure that the
// compiler in all builds, including jumbo builds, always knows about
// the specialization before the first template instance use. Using a
// template instance before its specialization is declared in a
// translation unit is a C++ error.
DECLARE_EXPORTED_UI_CLASS_PROPERTY_TYPE(VIEWS_EXPORT, gfx::Insets*)
DECLARE_EXPORTED_UI_CLASS_PROPERTY_TYPE(VIEWS_EXPORT, views::DialogDelegate*)
DECLARE_EXPORTED_UI_CLASS_PROPERTY_TYPE(VIEWS_EXPORT,
                                        views::HighlightPathGenerator*)
DECLARE_EXPORTED_UI_CLASS_PROPERTY_TYPE(VIEWS_EXPORT, views::FlexSpecification*)
DECLARE_EXPORTED_UI_CLASS_PROPERTY_TYPE(VIEWS_EXPORT, views::LayoutAlignment*)
DECLARE_EXPORTED_UI_CLASS_PROPERTY_TYPE(VIEWS_EXPORT, gfx::Size*)
DECLARE_EXPORTED_UI_CLASS_PROPERTY_TYPE(VIEWS_EXPORT, ui::ElementIdentifier)
DECLARE_EXPORTED_UI_CLASS_PROPERTY_TYPE(VIEWS_EXPORT, bool)
DECLARE_EXPORTED_UI_CLASS_PROPERTY_TYPE(VIEWS_EXPORT, views::View*)
DECLARE_EXPORTED_UI_CLASS_PROPERTY_TYPE(VIEWS_EXPORT, views::Widget*)

#endif  // UI_VIEWS_VIEW_CLASS_PROPERTIES_H_
