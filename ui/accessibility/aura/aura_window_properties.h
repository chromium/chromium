// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_AURA_AURA_WINDOW_PROPERTIES_H_
#define UI_ACCESSIBILITY_AURA_AURA_WINDOW_PROPERTIES_H_

#include <string>

#include "ui/accessibility/ax_enums.mojom-forward.h"
#include "ui/accessibility/ax_export.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"

namespace ui {

// Value is a serialized |AXTreeID| because code in //ui/aura/mus needs
// to serialize the window property, but //ui/aura cannot depend on
// //ui/accessibility and hence cannot know about the type AXTreeID.
// (Note: it would probably be better if this was a base::UnguessableToken
// instead of a std::string.)
AX_EXPORT extern const aura::WindowProperty<std::string*>* const kChildAXTreeID;

AX_EXPORT extern const aura::WindowProperty<ax::mojom::Role>* const
    kAXRoleOverride;

// Whether to force a window to be invisible with its children ignored. Used
// to hide the non-lock screen contents when the lock screen is shown.
AX_EXPORT extern const aura::WindowProperty<bool>* const
    kAXConsiderInvisibleAndIgnoreChildren;

}  // namespace ui

// Declare template specializations introduced here to make sure that the
// compiler knows about them before the first template instance use. Using a
// template instance before its specialization is declared in a translation unit
// is an error.
DECLARE_EXPORTED_UI_CLASS_PROPERTY_TYPE(AX_EXPORT, ax::mojom::Role*)

#endif  // UI_ACCESSIBILITY_AURA_AURA_WINDOW_PROPERTIES_H_
