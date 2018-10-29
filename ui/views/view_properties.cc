// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/view_properties.h"

#include "ui/base/hit_test.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

#if !defined(USE_AURA)
// aura_constants.cc also declared the bool and int[32_t]
// ClassProperty type.
DEFINE_EXPORTED_UI_CLASS_PROPERTY_TYPE(VIEWS_EXPORT, bool);
DEFINE_EXPORTED_UI_CLASS_PROPERTY_TYPE(VIEWS_EXPORT, int);
#endif

DEFINE_EXPORTED_UI_CLASS_PROPERTY_TYPE(VIEWS_EXPORT, gfx::Insets*);

DEFINE_EXPORTED_UI_CLASS_PROPERTY_TYPE(VIEWS_EXPORT,
                                       views::BubbleDialogDelegateView*);

DEFINE_EXPORTED_UI_CLASS_PROPERTY_TYPE(VIEWS_EXPORT, SkPath*);

namespace views {

DEFINE_UI_CLASS_PROPERTY_KEY(int, kHitTestComponentKey, HTNOWHERE);
DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(gfx::Insets, kMarginsKey, nullptr);
DEFINE_UI_CLASS_PROPERTY_KEY(views::BubbleDialogDelegateView*,
                             kAnchoredDialogKey,
                             nullptr);
DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(SkPath, kHighlightPathKey, nullptr);

}  // namespace views
