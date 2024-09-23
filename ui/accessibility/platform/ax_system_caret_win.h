// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_AX_SYSTEM_CARET_WIN_H_
#define UI_ACCESSIBILITY_PLATFORM_AX_SYSTEM_CARET_WIN_H_

#include <oleacc.h>
#include <wrl/client.h>

#include <type_traits>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_tree_data.h"
#include "ui/accessibility/platform/ax_platform_node_delegate.h"
#include "ui/accessibility/platform/ax_unique_id.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"

namespace ui {

class AXPlatformNodeWin;

// A class representing the position of the caret to assistive software on
// Windows. This is required because Chrome doesn't use the standard system
// caret and because some assistive software still relies on specific
// accessibility APIs to retrieve the caret position.
class COMPONENT_EXPORT(AX_PLATFORM) AXSystemCaretWin
    : private AXPlatformNodeDelegate {
 public:
  explicit AXSystemCaretWin(gfx::AcceleratedWidget event_target);

  AXSystemCaretWin(const AXSystemCaretWin&) = delete;
  AXSystemCaretWin& operator=(const AXSystemCaretWin&) = delete;

  ~AXSystemCaretWin() override;

  Microsoft::WRL::ComPtr<IAccessible> GetCaret() const;
  void MoveCaretTo(const gfx::Rect& bounds_physical_pixels);
  void Hide();

 private:
  // |AXPlatformNodeDelegate| members.
  const AXNodeData& GetData() const override;
  gfx::NativeViewAccessible GetParent() const override;
  gfx::Rect GetBoundsRect(const AXCoordinateSystem coordinate_system,
                          const AXClippingBehavior clipping_behavior,
                          AXOffscreenResult* offscreen_result) const override;
  gfx::AcceleratedWidget GetTargetForNativeAccessibilityEvent() override;
  bool ShouldIgnoreHoveredStateForTesting() override;
  AXPlatformNodeId GetUniqueId() const override;

  static void AXPlatformNodeWinDeleter(AXPlatformNodeWin* ptr);

  using deleter = std::integral_constant<
      decltype(AXSystemCaretWin::AXPlatformNodeWinDeleter)*,
      AXSystemCaretWin::AXPlatformNodeWinDeleter>;
  std::unique_ptr<AXPlatformNodeWin, deleter> caret_;
  gfx::AcceleratedWidget event_target_;
  AXNodeData data_;
  const AXUniqueId unique_id_{AXUniqueId::Create()};

  friend class AXPlatformNodeWin;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_AX_SYSTEM_CARET_WIN_H_
