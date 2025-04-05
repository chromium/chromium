// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ACCESSIBILITY_TREE_BROWSER_VIEWS_AX_MANAGER_H_
#define UI_VIEWS_ACCESSIBILITY_TREE_BROWSER_VIEWS_AX_MANAGER_H_

#include <map>
#include <memory>

#include "ui/accessibility/platform/ax_mode_observer.h"
#include "ui/accessibility/platform/ax_node_id_delegate.h"
#include "ui/accessibility/platform/ax_platform_tree_manager_delegate.h"
#include "ui/accessibility/platform/ax_unique_id.h"
#include "ui/views/accessibility/tree/views_ax_manager.h"

namespace ui {
class BrowserAccessibilityManager;
}  // namespace ui

namespace views {

class VIEWS_EXPORT BrowserViewsAXManager
    : public ViewsAXManager,
      public ui::AXNodeIdDelegate,
      public ui::AXPlatformTreeManagerDelegate,
      public ui::AXModeObserver {
 public:
  // Manages the lifetime of the BrowserViewsAXManager singleton.
  // Ensures the instance is created only once and properly cleaned up when no
  // longer needed.
  class VIEWS_EXPORT LifetimeHandle {
   public:
    using BrowserViewsAXManagerPassKey = base::PassKey<BrowserViewsAXManager>;
    LifetimeHandle(const LifetimeHandle&) = delete;
    LifetimeHandle& operator=(const LifetimeHandle&) = delete;

    explicit LifetimeHandle(BrowserViewsAXManagerPassKey);
    ~LifetimeHandle();

   private:
    std::unique_ptr<BrowserViewsAXManager> instance_;
  };

  using LifetimeHandlePassKey = base::PassKey<LifetimeHandle>;

  explicit BrowserViewsAXManager(LifetimeHandlePassKey);
  ~BrowserViewsAXManager() override;
  BrowserViewsAXManager(const BrowserViewsAXManager&) = delete;
  BrowserViewsAXManager& operator=(const BrowserViewsAXManager&) = delete;

  // Returns the single process-wide instance.
  static BrowserViewsAXManager* GetInstance();

  // Returns a new instance. Only one instance can be created.
  static std::unique_ptr<LifetimeHandle> Create();

  // ui::AXNodeIdDelegate:
  ui::AXPlatformNodeId GetOrCreateAXNodeUniqueId(
      ui::AXNodeID ax_node_id) override;
  void OnAXNodeDeleted(ui::AXNodeID ax_node_id) override;

  // ui::AXPlatformTreeManagerDelegate:
  void AccessibilityPerformAction(const ui::AXActionData& data) override;
  bool AccessibilityViewHasFocus() override;
  void AccessibilityViewSetFocus() override;
  gfx::Rect AccessibilityGetViewBounds() override;
  float AccessibilityGetDeviceScaleFactor() override;
  void UnrecoverableAccessibilityError() override;
  gfx::AcceleratedWidget AccessibilityGetAcceleratedWidget() override;
  gfx::NativeViewAccessible AccessibilityGetNativeViewAccessible() override;
  gfx::NativeViewAccessible AccessibilityGetNativeViewAccessibleForWindow()
      override;
  gfx::NativeViewAccessible GetRootViewNativeViewAccessible();
  void AccessibilityHitTest(
      const gfx::Point& point_in_frame_pixels,
      const ax::mojom::Event& opt_event_to_fire,
      int opt_request_id,
      base::OnceCallback<void(ui::AXPlatformTreeManager* hit_manager,
                              ui::AXNodeID hit_node_id)> opt_callback) override;
  gfx::NativeWindow GetTopLevelNativeWindow() override;
  bool CanFireAccessibilityEvents() const override;
  bool AccessibilityIsRootFrame() const override;
  bool ShouldSuppressAXLoadComplete() override;
  content::WebContentsAccessibility* AccessibilityGetWebContentsAccessibility()
      override;
  bool AccessibilityIsWebContentSource() override;

  // ui::AXModeObserver:
  void OnAXModeAdded(ui::AXMode mode) override;

 private:
  void Reset(bool reset_serializer) override;

  // Holds the generated AXTree of AXNodes for the views-based tree.
  std::unique_ptr<ui::BrowserAccessibilityManager> ax_tree_manager_;

  std::map<ui::AXNodeID, ui::AXUniqueId> ax_unique_ids_;
};

}  // namespace views

#endif  // UI_VIEWS_ACCESSIBILITY_TREE_BROWSER_VIEWS_AX_MANAGER_H_
