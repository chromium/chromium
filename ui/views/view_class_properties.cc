// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/view_class_properties.h"

#include "ui/base/hit_test.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout_types.h"

#if !defined(USE_AURA)
// aura_constants.cc also defines these types.
DEFINE_EXPORTED_UI_CLASS_PROPERTY_TYPE(VIEWS_EXPORT, int)
DEFINE_EXPORTED_UI_CLASS_PROPERTY_TYPE(VIEWS_EXPORT, gfx::Size*)
#endif

DEFINE_EXPORTED_UI_CLASS_PROPERTY_TYPE(VIEWS_EXPORT, gfx::Insets*)

DEFINE_EXPORTED_UI_CLASS_PROPERTY_TYPE(VIEWS_EXPORT, views::DialogDelegate*)

DEFINE_EXPORTED_UI_CLASS_PROPERTY_TYPE(VIEWS_EXPORT,
                                       views::BoxLayoutFlexSpecification*)
DEFINE_EXPORTED_UI_CLASS_PROPERTY_TYPE(VIEWS_EXPORT,
                                       views::HighlightPathGenerator*)
DEFINE_EXPORTED_UI_CLASS_PROPERTY_TYPE(VIEWS_EXPORT, views::FlexSpecification*)
DEFINE_EXPORTED_UI_CLASS_PROPERTY_TYPE(VIEWS_EXPORT, views::LayoutAlignment*)
DEFINE_EXPORTED_UI_CLASS_PROPERTY_TYPE(VIEWS_EXPORT, ui::ElementIdentifier)
DEFINE_EXPORTED_UI_CLASS_PROPERTY_TYPE(VIEWS_EXPORT, views::View*)
DEFINE_EXPORTED_UI_CLASS_PROPERTY_TYPE(VIEWS_EXPORT, views::Widget*)

namespace views {

DEFINE_UI_CLASS_PROPERTY_KEY(int, kHitTestComponentKey, HTNOWHERE)
DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(gfx::Insets, kMarginsKey, nullptr)
DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(gfx::Insets, kInternalPaddingKey, nullptr)
DEFINE_UI_CLASS_PROPERTY_KEY(views::DialogDelegate*,
                             kAnchoredDialogKey,
                             nullptr)
DEFINE_UI_CLASS_PROPERTY_KEY(views::Widget*, kWidgetForAnchoringKey, nullptr)
DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(BoxLayoutFlexSpecification,
                                   kBoxLayoutFlexKey,
                                   nullptr)
DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(views::HighlightPathGenerator,
                                   kHighlightPathGeneratorKey,
                                   nullptr)
DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(FlexSpecification, kFlexBehaviorKey, nullptr)
DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(LayoutAlignment,
                                   kCrossAxisAlignmentKey,
                                   nullptr)
DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(gfx::Size, kTableColAndRowSpanKey, nullptr)
DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(LayoutAlignment,
                                   kTableHorizAlignKey,
                                   nullptr)
DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(LayoutAlignment, kTableVertAlignKey, nullptr)
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kViewIgnoredByLayoutKey, false)
DEFINE_UI_CLASS_PROPERTY_KEY(ui::ElementIdentifier,
                             kElementIdentifierKey,
                             ui::ElementIdentifier())

}  // namespace views
