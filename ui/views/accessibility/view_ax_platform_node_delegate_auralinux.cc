// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/accessibility/view_ax_platform_node_delegate_auralinux.h"

#include <algorithm>
#include <memory>
#include <vector>

#include "base/memory/singleton.h"
#include "base/stl_util.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/platform/ax_platform_node_auralinux.h"
#include "ui/accessibility/platform/ax_platform_node_delegate_base.h"
#include "ui/aura/window.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/view.h"
#include "ui/views/views_delegate.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace views {

namespace {

// ATK requires that we have a single root "application" object that's the
// owner of all other windows. This is a simple class that implements the
// AXPlatformNodeDelegate interface so we can create such an application
// object. Every time we create an accessibility object for a View, we add its
// top-level widget to a vector so we can return the list of all top-level
// windows as children of this application object.
class AuraLinuxApplication : public ui::AXPlatformNodeDelegateBase,
                             public WidgetObserver,
                             public aura::WindowObserver {
 public:
  AuraLinuxApplication(const AuraLinuxApplication&) = delete;
  AuraLinuxApplication& operator=(const AuraLinuxApplication&) = delete;

  // Get the single instance of this class.
  static AuraLinuxApplication* GetInstance() {
    return base::Singleton<AuraLinuxApplication>::get();
  }

  // Called every time we create a new accessibility on a View.
  // Add the top-level widget to our registry so that we can enumerate all
  // top-level widgets.
  void RegisterWidget(Widget* widget) {
    if (!widget)
      return;

    widget = widget->GetTopLevelWidget();
    if (base::Contains(widgets_, widget))
      return;

    widgets_.push_back(widget);
    widget->AddObserver(this);

    aura::Window* window = widget->GetNativeWindow();
    if (!window)
      return;
    window->AddObserver(this);
  }

  gfx::NativeViewAccessible GetNativeViewAccessible() override {
    return platform_node_->GetNativeViewAccessible();
  }

  const ui::AXUniqueId& GetUniqueId() const override { return unique_id_; }

  // WidgetObserver:

  void OnWidgetDestroying(Widget* widget) override {
    auto iter = std::find(widgets_.begin(), widgets_.end(), widget);
    if (iter != widgets_.end())
      widgets_.erase(iter);
  }

  void OnWindowVisibilityChanged(aura::Window* window, bool visible) override {
    for (Widget* widget : widgets_) {
      if (widget->GetNativeWindow() != window)
        continue;

      View* root_view = widget->GetRootView();
      if (!root_view)
        continue;

      root_view->NotifyAccessibilityEvent(
          ax::mojom::Event::kWindowVisibilityChanged, true);
    }
  }

  // ui::AXPlatformNodeDelegate:

  const ui::AXNodeData& GetData() const override { return data_; }

  int GetChildCount() override { return static_cast<int>(widgets_.size()); }

  gfx::NativeViewAccessible ChildAtIndex(int index) override {
    if (index < 0 || index >= GetChildCount())
      return nullptr;

    Widget* widget = widgets_[index];
    CHECK(widget);
    return widget->GetRootView()->GetNativeViewAccessible();
  }

 private:
  friend struct base::DefaultSingletonTraits<AuraLinuxApplication>;

  AuraLinuxApplication() {
    data_.role = ax::mojom::Role::kApplication;
    platform_node_ = ui::AXPlatformNode::Create(this);
    data_.AddStringAttribute(
        ax::mojom::StringAttribute::kName,
        ViewsDelegate::GetInstance()->GetApplicationName());
    ui::AXPlatformNodeAuraLinux::SetApplication(platform_node_);
    ui::AXPlatformNodeAuraLinux::StaticInitialize();
  }

  ~AuraLinuxApplication() override {
    platform_node_->Destroy();
    platform_node_ = nullptr;
  }

  ui::AXPlatformNode* platform_node_;
  ui::AXNodeData data_;
  ui::AXUniqueId unique_id_;
  std::vector<Widget*> widgets_;
};

}  // namespace

// static
std::unique_ptr<ViewAccessibility> ViewAccessibility::Create(View* view) {
  AuraLinuxApplication::GetInstance()->RegisterWidget(view->GetWidget());
  return std::make_unique<ViewAXPlatformNodeDelegateAuraLinux>(view);
}

ViewAXPlatformNodeDelegateAuraLinux::ViewAXPlatformNodeDelegateAuraLinux(
    View* view)
    : ViewAXPlatformNodeDelegate(view) {
  view->AddObserver(this);
}

ViewAXPlatformNodeDelegateAuraLinux::~ViewAXPlatformNodeDelegateAuraLinux() =
    default;

gfx::NativeViewAccessible ViewAXPlatformNodeDelegateAuraLinux::GetParent() {
  gfx::NativeViewAccessible parent = ViewAXPlatformNodeDelegate::GetParent();
  if (!parent)
    parent = AuraLinuxApplication::GetInstance()->GetNativeViewAccessible();
  return parent;
}

void ViewAXPlatformNodeDelegateAuraLinux::OnViewHierarchyChanged(
    views::View* observed_view,
    const views::ViewHierarchyChangedDetails& details) {
  if (view() != details.child || !details.is_add)
    return;
  static_cast<ui::AXPlatformNodeAuraLinux*>(ax_platform_node())
      ->OnParentChanged();
}

}  // namespace views
