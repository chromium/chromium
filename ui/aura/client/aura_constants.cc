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

DEFINE_EXPORTED_UI_CLASS_PROPERTY_TYPE(AURA_EXPORT, base::TimeDelta)
DEFINE_EXPORTED_UI_CLASS_PROPERTY_TYPE(AURA_EXPORT, base::UnguessableToken*)
DEFINE_EXPORTED_UI_CLASS_PROPERTY_TYPE(AURA_EXPORT, std::u16string*)
DEFINE_EXPORTED_UI_CLASS_PROPERTY_TYPE(AURA_EXPORT, ui::mojom::ModalType)
DEFINE_EXPORTED_UI_CLASS_PROPERTY_TYPE(AURA_EXPORT, ui::OwnedWindowAnchor*)
DEFINE_EXPORTED_UI_CLASS_PROPERTY_TYPE(AURA_EXPORT, ui::ZOrderLevel)
DEFINE_EXPORTED_UI_CLASS_PROPERTY_TYPE(AURA_EXPORT, gfx::ImageSkia*)
DEFINE_EXPORTED_UI_CLASS_PROPERTY_TYPE(AURA_EXPORT, gfx::NativeViewAccessible)
DEFINE_EXPORTED_UI_CLASS_PROPERTY_TYPE(AURA_EXPORT, gfx::Rect*)
DEFINE_EXPORTED_UI_CLASS_PROPERTY_TYPE(AURA_EXPORT, gfx::Size*)
DEFINE_EXPORTED_UI_CLASS_PROPERTY_TYPE(AURA_EXPORT, gfx::SizeF*)
DEFINE_EXPORTED_UI_CLASS_PROPERTY_TYPE(AURA_EXPORT, std::string*)
DEFINE_EXPORTED_UI_CLASS_PROPERTY_TYPE(AURA_EXPORT, ui::mojom::WindowShowState)
DEFINE_EXPORTED_UI_CLASS_PROPERTY_TYPE(AURA_EXPORT, void*)
DEFINE_EXPORTED_UI_CLASS_PROPERTY_TYPE(AURA_EXPORT, SkColor)
DEFINE_EXPORTED_UI_CLASS_PROPERTY_TYPE(AURA_EXPORT, int32_t)
DEFINE_EXPORTED_UI_CLASS_PROPERTY_TYPE(AURA_EXPORT, int64_t)
DEFINE_EXPORTED_UI_CLASS_PROPERTY_TYPE(AURA_EXPORT, uint64_t)
DEFINE_EXPORTED_UI_CLASS_PROPERTY_TYPE(AURA_EXPORT, aura::client::FocusClient*)
DEFINE_EXPORTED_UI_CLASS_PROPERTY_TYPE(AURA_EXPORT, aura::Window*)
DEFINE_EXPORTED_UI_CLASS_PROPERTY_TYPE(AURA_EXPORT, std::vector<aura::Window*>*)

namespace aura {
namespace client {

// Alphabetical sort.

DEFINE_UI_CLASS_PROPERTY_KEY(bool,
                             kAccessibilityTouchExplorationPassThrough,
                             false)
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kActivateOnPointerKey, true)
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kAnimationsDisabledKey, false)
DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(gfx::ImageSkia, kAppIconKey, nullptr)
DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(gfx::SizeF, kAspectRatio, nullptr)
DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(gfx::ImageSkia, kAvatarIconKey, nullptr)
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kWindowLayerDrawn, false)
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kConstrainedWindowKey, false)
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kCreatedByUserGesture, false)
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kDrawAttentionKey, false)
DEFINE_UI_CLASS_PROPERTY_KEY(FocusClient*, kFocusClientKey, nullptr)
DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(gfx::Rect, kHeadlessBoundsKey, nullptr)
DEFINE_UI_CLASS_PROPERTY_KEY(Window*, kHostWindowKey, nullptr)
DEFINE_UI_CLASS_PROPERTY_KEY(Window*, kChildModalParentKey, nullptr)
DEFINE_UI_CLASS_PROPERTY_KEY(ui::mojom::ModalType,
                             kModalKey,
                             ui::mojom::ModalType::kNone)
DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(std::string, kNameKey, nullptr)
DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(ui::OwnedWindowAnchor,
                                   kOwnedWindowAnchor,
                                   nullptr)

DEFINE_UI_CLASS_PROPERTY_KEY(bool, kUseWindowBoundsForShadow, true)

DEFINE_UI_CLASS_PROPERTY_KEY(gfx::NativeViewAccessible,
                             kParentNativeViewAccessibleKey,
                             nullptr)
DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(gfx::Size, kPreferredSize, nullptr)
DEFINE_UI_CLASS_PROPERTY_KEY(int, kResizeBehaviorKey, kResizeBehaviorCanResize)
DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(gfx::Rect, kRestoreBoundsKey, nullptr)
DEFINE_UI_CLASS_PROPERTY_KEY(ui::mojom::WindowShowState,
                             kShowStateKey,
                             ui::mojom::WindowShowState::kDefault)
DEFINE_UI_CLASS_PROPERTY_KEY(int64_t,
                             kFullscreenTargetDisplayIdKey,
                             display::kInvalidDisplayId)
DEFINE_UI_CLASS_PROPERTY_KEY(ui::mojom::WindowShowState,
                             kRestoreShowStateKey,
                             ui::mojom::WindowShowState::kNormal)
DEFINE_UI_CLASS_PROPERTY_KEY(float, kRasterScale, 1.0f)
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kIsRestoringKey, false)
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kSkipImeProcessing, false)
DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(std::u16string, kTitleKey, nullptr)
DEFINE_UI_CLASS_PROPERTY_KEY(int, kTopViewInset, 0)
DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(gfx::ImageSkia, kWindowIconKey, nullptr)
DEFINE_UI_CLASS_PROPERTY_KEY(int, kWindowCornerRadiusKey, -1)
DEFINE_UI_CLASS_PROPERTY_KEY(int,
                             kWindowWorkspaceKey,
                             kWindowWorkspaceUnassignedWorkspace)
DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(std::string, kDeskUuidKey, nullptr)
DEFINE_UI_CLASS_PROPERTY_KEY(ui::ZOrderLevel,
                             kZOrderingKey,
                             ui::ZOrderLevel::kNormal)

}  // namespace client
}  // namespace aura
