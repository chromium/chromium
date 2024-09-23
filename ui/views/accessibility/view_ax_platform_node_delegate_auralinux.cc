// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/accessibility/view_ax_platform_node_delegate_auralinux.h"

#include <memory>
#include <vector>

#include "base/containers/contains.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/ranges/algorithm.h"
#include "base/scoped_multi_source_observation.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/platform/ax_platform_node_auralinux.h"
#include "ui/accessibility/platform/ax_platform_node_delegate.h"
#include "ui/accessibility/platform/ax_unique_id.h"
#include "ui/aura/window.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/accessibility/views_utilities_aura.h"
#include "ui/views/view.h"
#include "ui/views/views_delegate.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"
#include "ui/wm/core/window_util.h"

namespace views {

namespace {

// Return the widget of any parent window of |widget|, first checking for
// transient parent windows.
Widget* GetWidgetOfParentWindowIncludingTransient(Widget* widget) {
  if (!widget)
    return nullptr;

  aura::Window* window = widget->GetNativeWindow();
  if (!window)
    return nullptr;

  // Look for an ancestor window with a Widget, and if found, return
  // the NativeViewAccessible for its RootView.
  aura::Window* ancestor_window = GetWindowParentIncludingTransient(window);
  if (!ancestor_window)
    return nullptr;

  return Widget::GetWidgetForNativeView(ancestor_window);
}

// Return the toplevel widget ancestor of |widget|, including widgets of
// parents of transient windows.
Widget* GetToplevelWidgetIncludingTransientWindows(Widget* widget) {
  widget = widget->GetTopLevelWidget();
  if (Widget* parent_widget = GetWidgetOfParentWindowIncludingTransient(widget))
    return GetToplevelWidgetIncludingTransientWindows(parent_widget);
  return widget;
}

// ATK requires that we have a single root "application" object that's the
// owner of all other windows. This is a simple class that implements the
// AXPlatformNodeDelegate interface so we can create such an application
// object. Every time we create an accessibility object for a View, we add its
// top-level widget to a vector so we can return the list of all top-level
// windows as children of this application object.
class AuraLinuxApplication : public ui::AXPlatformNodeDelegate,
                             public WidgetObserver,
                             public aura::WindowObserver {
 public:
  AuraLinuxApplication(const AuraLinuxApplication&) = delete;
  AuraLinuxApplication& operator=(const AuraLinuxApplication&) = delete;

  // Get the single instance of this class.
  static AuraLinuxApplication& GetInstance() {
    static base::NoDestructor<AuraLinuxApplication> instance;
    return *instance;
  }

  // Called every time we create a new accessibility on a View.
  // Add the top-level widget to our registry so that we can enumerate all
  // top-level widgets.
  void RegisterWidget(Widget* widget) {
    if (!widget)
      return;

    widget = GetToplevelWidgetIncludingTransientWindows(widget);
    if (!widget || !widget->native_widget() ||
        base::Contains(widgets_, widget)) {
      return;
    }

    widgets_.push_back(widget);
    widget_observations_.AddObservation(widget);

    aura::Window* window = widget->GetNativeWindow();
    if (window)
      window_observations_.AddObservation(window);
  }

  gfx::NativeViewAccessible GetNativeViewAccessible() override {
    return ax_platform_node_->GetNativeViewAccessible();
  }

  ui::AXPlatformNodeId GetUniqueId() const override { return unique_id_; }

  // WidgetObserver:

  void OnWidgetDestroying(Widget* widget) override {
    widget_observations_.RemoveObservation(widget);

    aura::Window* window = widget->GetNativeWindow();
    if (window && window_observations_.IsObservingSource(window))
      window_observations_.RemoveObservation(window);

    auto iter = base::ranges::find(widgets_, widget);
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

  const ui::AXNodeData& GetData() const override {
    // Despite the fact that the comment above
    // `views::ViewsDelegate::GetInstance()` says that a nullptr check is not
    // needed, we discovered that the delegate instance may be nullptr during
    // test setup. Since the application name does not change, we can set it
    // only once and avoid setting it every time our accessibility data is
    // retrieved.
    if (data_.GetStringAttribute(ax::mojom::StringAttribute::kName).empty() &&
        ViewsDelegate::GetInstance()) {
      data_.SetNameChecked(ViewsDelegate::GetInstance()->GetApplicationName());
    }

    return data_;
  }

  size_t GetChildCount() const override { return widgets_.size(); }

  gfx::NativeViewAccessible ChildAtIndex(size_t index) const override {
    if (index >= GetChildCount())
      return nullptr;

    Widget* widget = widgets_[index];
    CHECK(widget);
    return widget->GetRootView()->GetNativeViewAccessible();
  }

  bool IsChildOfLeaf() const override {
    // TODO(crbug.com/40702759): Needed to prevent endless loops only on Linux
    // ATK.
    return false;
  }

 private:
  friend class base::NoDestructor<AuraLinuxApplication>;

  AuraLinuxApplication() {
    data_.id = unique_id_.Get();
    data_.role = ax::mojom::Role::kApplication;
    data_.AddState(ax::mojom::State::kFocusable);
    ax_platform_node_ = ui::AXPlatformNode::Create(this);
    DCHECK(ax_platform_node_);
    ui::AXPlatformNodeAuraLinux::SetApplication(ax_platform_node_);
    ui::AXPlatformNodeAuraLinux::StaticInitialize();
  }

  ~AuraLinuxApplication() override {
    ax_platform_node_->Destroy();
    ax_platform_node_ = nullptr;
  }

  // TODO(nektar): Make this into a const pointer so that it can't be set
  // outside the class's constructor.
  raw_ptr<ui::AXPlatformNode> ax_platform_node_;
  const ui::AXUniqueId unique_id_{ui::AXUniqueId::Create()};
  mutable ui::AXNodeData data_;
  std::vector<raw_ptr<Widget, VectorExperimental>> widgets_;
  base::ScopedMultiSourceObservation<Widget, WidgetObserver>
      widget_observations_{this};
  base::ScopedMultiSourceObservation<aura::Window, aura::WindowObserver>
      window_observations_{this};
};

}  // namespace

// static
std::unique_ptr<ViewAccessibility> ViewAccessibility::Create(View* view) {
  AuraLinuxApplication::GetInstance().RegisterWidget(view->GetWidget());

  auto result = std::make_unique<ViewAXPlatformNodeDelegateAuraLinux>(view);
  result->Init();
  return result;
}

ViewAXPlatformNodeDelegateAuraLinux::ViewAXPlatformNodeDelegateAuraLinux(
    View* view)
    : ViewAXPlatformNodeDelegate(view) {}

void ViewAXPlatformNodeDelegateAuraLinux::Init() {
  ViewAXPlatformNodeDelegate::Init();

  view_observation_.Observe(view());
}

ViewAXPlatformNodeDelegateAuraLinux::~ViewAXPlatformNodeDelegateAuraLinux() {
  view_observation_.Reset();
}

gfx::NativeViewAccessible ViewAXPlatformNodeDelegateAuraLinux::GetParent()
    const {
  if (gfx::NativeViewAccessible parent =
          ViewAXPlatformNodeDelegate::GetParent()) {
    return parent;
  }

  Widget* parent_widget =
      GetWidgetOfParentWindowIncludingTransient(view()->GetWidget());
  if (parent_widget)
    return parent_widget->GetRootView()->GetNativeViewAccessible();

  return AuraLinuxApplication::GetInstance().GetNativeViewAccessible();
}

bool ViewAXPlatformNodeDelegateAuraLinux::IsChildOfLeaf() const {
  // TODO(crbug.com/40702759): Needed to prevent endless loops only on Linux
  // ATK.
  return false;
}

void ViewAXPlatformNodeDelegateAuraLinux::OnViewHierarchyChanged(
    View* observed_view,
    const ViewHierarchyChangedDetails& details) {
  if (view() != details.child || !details.is_add)
    return;
  static_cast<ui::AXPlatformNodeAuraLinux*>(ax_platform_node())
      ->OnParentChanged();
}

}  // namespace views
