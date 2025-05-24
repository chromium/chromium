// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/client/aura_constants.h"

#include "ui/base/class_property.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/base/owned_window_anchor.h"
#include "ui/base/ui_base_types.h"
#include "ui/display/types/display_constants.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image_skia.h"

DEFINE_EXPORTED_UI_CLASS_PROPERTY_TYPE(AURA_EXPORT, aura::client::FocusClient*)
DEFINE_EXPORTED_UI_CLASS_PROPERTY_TYPE(AURA_EXPORT, aura::Window*)
DEFINE_EXPORTED_UI_CLASS_PROPERTY_TYPE(AURA_EXPORT, gfx::ImageSkia*)
DEFINE_EXPORTED_UI_CLASS_PROPERTY_TYPE(AURA_EXPORT, gfx::NativeViewAccessible)
DEFINE_EXPORTED_UI_CLASS_PROPERTY_TYPE(AURA_EXPORT, gfx::Rect*)
DEFINE_EXPORTED_UI_CLASS_PROPERTY_TYPE(AURA_EXPORT, gfx::Size*)
DEFINE_EXPORTED_UI_CLASS_PROPERTY_TYPE(AURA_EXPORT, gfx::SizeF*)
DEFINE_EXPORTED_UI_CLASS_PROPERTY_TYPE(AURA_EXPORT, int64_t)
DEFINE_EXPORTED_UI_CLASS_PROPERTY_TYPE(AURA_EXPORT, std::string*)
DEFINE_EXPORTED_UI_CLASS_PROPERTY_TYPE(AURA_EXPORT, ui::mojom::ModalType)
DEFINE_EXPORTED_UI_CLASS_PROPERTY_TYPE(AURA_EXPORT, ui::mojom::WindowShowState)
DEFINE_EXPORTED_UI_CLASS_PROPERTY_TYPE(AURA_EXPORT, ui::OwnedWindowAnchor*)
DEFINE_EXPORTED_UI_CLASS_PROPERTY_TYPE(AURA_EXPORT, ui::ZOrderLevel)

namespace aura {
namespace client {

// Alphabetical sort.

DEFINE_UI_CLASS_PROPERTY_KEY(bool,
                             kAccessibilityTouchExplorationPassThrough,
                             false)
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kActivateOnPointerKey, true)
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kAnimationsDisabledKey, false)
DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(gfx::ImageSkia, kAppIconKey)
DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(gfx::SizeF, kAspectRatio)
DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(gfx::ImageSkia, kAvatarIconKey)
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kWindowLayerDrawn, false)
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kConstrainedWindowKey, false)
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kCreatedByUserGesture, false)
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kDrawAttentionKey, false)
DEFINE_UI_CLASS_PROPERTY_KEY(FocusClient*, kFocusClientKey, nullptr)
DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(gfx::Rect, kHeadlessBoundsKey)
DEFINE_UI_CLASS_PROPERTY_KEY(Window*, kHostWindowKey, nullptr)
DEFINE_UI_CLASS_PROPERTY_KEY(Window*, kChildModalParentKey, nullptr)
DEFINE_UI_CLASS_PROPERTY_KEY(ui::mojom::ModalType,
                             kModalKey,
                             ui::mojom::ModalType::kNone)
DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(std::string, kNameKey)
DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(ui::OwnedWindowAnchor, kOwnedWindowAnchor)

DEFINE_UI_CLASS_PROPERTY_KEY(bool, kUseWindowBoundsForShadow, true)

DEFINE_UI_CLASS_PROPERTY_KEY(gfx::NativeViewAccessible,
                             kParentNativeViewAccessibleKey,
                             nullptr)
DEFINE_UI_CLASS_PROPERTY_KEY(int, kResizeBehaviorKey, kResizeBehaviorCanResize)
DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(gfx::Rect, kRestoreBoundsKey)
DEFINE_UI_CLASS_PROPERTY_KEY(ui::mojom::WindowShowState,
                             kShowStateKey,
                             ui::mojom::WindowShowState::kDefault)
DEFINE_UI_CLASS_PROPERTY_KEY(int64_t,
                             kFullscreenTargetDisplayIdKey,
                             display::kInvalidDisplayId)
DEFINE_UI_CLASS_PROPERTY_KEY(ui::mojom::WindowShowState,
                             kRestoreShowStateKey,
                             ui::mojom::WindowShowState::kNormal)
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kIsRestoringKey, false)
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kSkipImeProcessing, false)
DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(std::u16string, kTitleKey)
DEFINE_UI_CLASS_PROPERTY_KEY(int, kTopViewInset, 0)
DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(gfx::ImageSkia, kWindowIconKey)
DEFINE_UI_CLASS_PROPERTY_KEY(int, kWindowCornerRadiusKey, -1)
DEFINE_UI_CLASS_PROPERTY_KEY(int,
                             kWindowWorkspaceKey,
                             kWindowWorkspaceUnassignedWorkspace)
DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(std::string, kDeskUuidKey)
DEFINE_UI_CLASS_PROPERTY_KEY(ui::ZOrderLevel,
                             kZOrderingKey,
                             ui::ZOrderLevel::kNormal)

}  // namespace client
}  // namespace aura
