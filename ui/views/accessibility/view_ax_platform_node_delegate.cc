// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/accessibility/view_ax_platform_node_delegate.h"

#include <map>
#include <memory>

#include "base/lazy_instance.h"
#include "base/threading/thread_task_runner_handle.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_role_properties.h"
#include "ui/accessibility/ax_tree_data.h"
#include "ui/accessibility/platform/ax_platform_node.h"
#include "ui/accessibility/platform/ax_unique_id.h"
#include "ui/events/event_utils.h"
#include "ui/views/accessibility/view_accessibility_utils.h"
#include "ui/views/controls/native/native_view_host.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace views {

namespace {

base::LazyInstance<std::map<int32_t, ui::AXPlatformNode*>>::Leaky
    g_unique_id_to_ax_platform_node = LAZY_INSTANCE_INITIALIZER;

// Information required to fire a delayed accessibility event.
struct QueuedEvent {
  ax::mojom::Event type;
  int32_t node_id;
};

base::LazyInstance<std::vector<QueuedEvent>>::Leaky g_event_queue =
    LAZY_INSTANCE_INITIALIZER;

bool g_is_queueing_events = false;

bool IsAccessibilityFocusableWhenEnabled(View* view) {
  return view->focus_behavior() != View::FocusBehavior::NEVER &&
         view->IsDrawn();
}

// Used to determine if a View should be ignored by accessibility clients by
// being a non-keyboard-focusable child of a keyboard-focusable ancestor. E.g.,
// LabelButtons contain Labels, but a11y should just show that there's a button.
bool IsViewUnfocusableDescendantOfFocusableAncestor(View* view) {
  if (IsAccessibilityFocusableWhenEnabled(view))
    return false;

  while (view->parent()) {
    view = view->parent();
    if (IsAccessibilityFocusableWhenEnabled(view))
      return true;
  }
  return false;
}

ui::AXPlatformNode* FromNativeWindow(gfx::NativeWindow native_window) {
  Widget* widget = Widget::GetWidgetForNativeWindow(native_window);
  if (!widget)
    return nullptr;

  View* view = widget->GetRootView();
  if (!view)
    return nullptr;

  gfx::NativeViewAccessible native_view_accessible =
      view->GetNativeViewAccessible();
  if (!native_view_accessible)
    return nullptr;

  return ui::AXPlatformNode::FromNativeViewAccessible(native_view_accessible);
}

ui::AXPlatformNode* PlatformNodeFromNodeID(int32_t id) {
  // Note: For Views, node IDs and unique IDs are the same - but that isn't
  // necessarily true for all AXPlatformNodes.
  auto it = g_unique_id_to_ax_platform_node.Get().find(id);

  if (it == g_unique_id_to_ax_platform_node.Get().end())
    return nullptr;

  return it->second;
}

void FireEvent(QueuedEvent event) {
  ui::AXPlatformNode* node = PlatformNodeFromNodeID(event.node_id);
  if (node)
    node->NotifyAccessibilityEvent(event.type);
}

void FlushQueue() {
  DCHECK(g_is_queueing_events);
  for (QueuedEvent event : g_event_queue.Get())
    FireEvent(event);
  g_is_queueing_events = false;
  g_event_queue.Get().clear();
}

}  // namespace

// static
int ViewAXPlatformNodeDelegate::menu_depth_ = 0;

ViewAXPlatformNodeDelegate::ViewAXPlatformNodeDelegate(View* view)
    : ViewAccessibility(view) {
  ax_node_ = ui::AXPlatformNode::Create(this);
  DCHECK(ax_node_);

  static bool first_time = true;
  if (first_time) {
    ui::AXPlatformNode::RegisterNativeWindowHandler(
        base::BindRepeating(&FromNativeWindow));
    first_time = false;
  }

  g_unique_id_to_ax_platform_node.Get()[GetUniqueId().Get()] = ax_node_;
}

ViewAXPlatformNodeDelegate::~ViewAXPlatformNodeDelegate() {
  if (ui::AXPlatformNode::GetPopupFocusOverride() == GetNativeObject())
    ui::AXPlatformNode::SetPopupFocusOverride(nullptr);

  g_unique_id_to_ax_platform_node.Get().erase(GetUniqueId().Get());
  ax_node_->Destroy();
}

gfx::NativeViewAccessible ViewAXPlatformNodeDelegate::GetNativeObject() {
  return ax_node_->GetNativeViewAccessible();
}

void ViewAXPlatformNodeDelegate::NotifyAccessibilityEvent(
    ax::mojom::Event event_type) {
  if (g_is_queueing_events) {
    g_event_queue.Get().push_back({event_type, GetUniqueId().Get()});
    return;
  }

  ax_node_->NotifyAccessibilityEvent(event_type);

  // Some events have special handling.
  switch (event_type) {
    case ax::mojom::Event::kMenuStart:
      OnMenuStart();
      break;
    case ax::mojom::Event::kMenuEnd:
      OnMenuEnd();
      break;
    case ax::mojom::Event::kSelection:
      if (menu_depth_ && ui::IsMenuItem(GetData().role))
        OnMenuItemActive();
      break;
    case ax::mojom::Event::kFocusContext: {
      // A focus context event is intended to send a focus event and a delay
      // before the next focus event. It makes sense to delay the entire next
      // synchronous batch of next events so that ordering remains the same.
      // Begin queueing subsequent events and flush queue asynchronously.
      g_is_queueing_events = true;
      base::OnceCallback<void()> cb = base::BindOnce(&FlushQueue);
      base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, std::move(cb));
      break;
    }
    default:
      break;
  }
}

#if defined(OS_MACOSX)
void ViewAXPlatformNodeDelegate::AnnounceText(base::string16& text) {
  ax_node_->AnnounceText(text);
}
#endif

void ViewAXPlatformNodeDelegate::OnMenuItemActive() {
  // When a native menu is shown and has an item selected, treat it and the
  // currently selected item as focused, even though the actual focus is in the
  // browser's currently focused textfield.
  ui::AXPlatformNode::SetPopupFocusOverride(
      ax_node_->GetNativeViewAccessible());
}

void ViewAXPlatformNodeDelegate::OnMenuStart() {
  ++menu_depth_;
}

void ViewAXPlatformNodeDelegate::OnMenuEnd() {
  // When a native menu is hidden, restore accessibility focus to the current
  // focus in the document.
  if (menu_depth_ >= 1)
    --menu_depth_;
  if (menu_depth_ == 0)
    ui::AXPlatformNode::SetPopupFocusOverride(nullptr);
}

// ui::AXPlatformNodeDelegate

const ui::AXNodeData& ViewAXPlatformNodeDelegate::GetData() const {
  // Clear it, then populate it.
  data_ = ui::AXNodeData();
  GetAccessibleNodeData(&data_);

  // View::IsDrawn is true if a View is visible and all of its ancestors are
  // visible too, since invisibility inherits.
  //
  // TODO(dmazzoni): Maybe consider moving this to ViewAccessibility?
  // This will require ensuring that Chrome OS invalidates the whole
  // subtree when a View changes its visibility state.
  if (!view()->IsDrawn())
    data_.AddState(ax::mojom::State::kInvisible);

  // Make sure this element is excluded from the a11y tree if there's a
  // focusable parent. All keyboard focusable elements should be leaf nodes.
  // Exceptions to this rule will themselves be accessibility focusable.
  //
  // TODO(dmazzoni): this code was added to support MacViews acccessibility,
  // because we needed a way to mark a View as a leaf node in the
  // accessibility tree. We need to replace this with a cross-platform
  // solution that works for ChromeVox, too, and move it to ViewAccessibility.
  if (IsViewUnfocusableDescendantOfFocusableAncestor(view()))
    data_.role = ax::mojom::Role::kIgnored;

  return data_;
}

int ViewAXPlatformNodeDelegate::GetChildCount() {
  if (IsLeaf())
    return 0;

  if (virtual_child_count())
    return virtual_child_count();

  int child_count = view()->child_count();

  std::vector<Widget*> child_widgets;
  bool is_tab_modal_showing;
  PopulateChildWidgetVector(&child_widgets, &is_tab_modal_showing);
  if (is_tab_modal_showing) {
    DCHECK_EQ(child_widgets.size(), 1ULL);
    return 1;
  }
  child_count += child_widgets.size();

  return child_count;
}

gfx::NativeViewAccessible ViewAXPlatformNodeDelegate::ChildAtIndex(int index) {
  DCHECK_GE(index, 0) << "Child indices should be greater or equal to 0.";
  DCHECK_LT(index, GetChildCount())
      << "Child indices should be less than the child count.";
  if (IsLeaf())
    return nullptr;

  if (virtual_child_count())
    return virtual_child_at(index)->GetNativeObject();

  // If this is a root view, our widget might have child widgets. Include
  std::vector<Widget*> child_widgets;
  bool is_tab_modal_showing;
  PopulateChildWidgetVector(&child_widgets, &is_tab_modal_showing);

  // If a visible tab modal dialog is present, ignore |index| and return the
  // dialog.
  if (is_tab_modal_showing) {
    DCHECK_EQ(child_widgets.size(), 1ULL);
    return child_widgets[0]->GetRootView()->GetNativeViewAccessible();
  }

  int child_widget_count = static_cast<int>(child_widgets.size());

  if (index < view()->child_count()) {
    return view()->child_at(index)->GetNativeViewAccessible();
  } else if (index < view()->child_count() + child_widget_count) {
    Widget* child_widget = child_widgets[index - view()->child_count()];
    return child_widget->GetRootView()->GetNativeViewAccessible();
  }

  return nullptr;
}

gfx::NativeWindow ViewAXPlatformNodeDelegate::GetTopLevelWidget() {
  if (view()->GetWidget())
    return view()->GetWidget()->GetTopLevelWidget()->GetNativeWindow();
  return nullptr;
}

gfx::NativeViewAccessible ViewAXPlatformNodeDelegate::GetParent() {
  if (view()->parent())
    return view()->parent()->GetNativeViewAccessible();

  if (Widget* widget = view()->GetWidget()) {
    Widget* top_widget = widget->GetTopLevelWidget();
    if (top_widget && widget != top_widget && top_widget->GetRootView())
      return top_widget->GetRootView()->GetNativeViewAccessible();
  }

  return nullptr;
}

gfx::Rect ViewAXPlatformNodeDelegate::GetClippedScreenBoundsRect() const {
  // We could optionally add clipping here if ever needed.
  return view()->GetBoundsInScreen();
}

gfx::Rect ViewAXPlatformNodeDelegate::GetUnclippedScreenBoundsRect() const {
  return view()->GetBoundsInScreen();
}

gfx::NativeViewAccessible ViewAXPlatformNodeDelegate::HitTestSync(int x,
                                                                  int y) {
  if (!view() || !view()->GetWidget())
    return nullptr;

  if (IsLeaf())
    return GetNativeObject();

  // Search child widgets first, since they're on top in the z-order.
  std::vector<Widget*> child_widgets;
  bool is_tab_modal_showing;
  PopulateChildWidgetVector(&child_widgets, &is_tab_modal_showing);
  for (Widget* child_widget : child_widgets) {
    View* child_root_view = child_widget->GetRootView();
    gfx::Point point(x, y);
    View::ConvertPointFromScreen(child_root_view, &point);
    if (child_root_view->HitTestPoint(point))
      return child_root_view->GetNativeViewAccessible();
  }

  gfx::Point point(x, y);
  View::ConvertPointFromScreen(view(), &point);
  if (!view()->HitTestPoint(point))
    return nullptr;

  // Check if the point is within any of the immediate children of this
  // view. We don't have to search further because AXPlatformNode will
  // do a recursive hit test if we return anything other than |this| or NULL.
  for (int i = view()->child_count() - 1; i >= 0; --i) {
    View* child_view = view()->child_at(i);
    if (!child_view->visible())
      continue;

    gfx::Point point_in_child_coords(point);
    view()->ConvertPointToTarget(view(), child_view, &point_in_child_coords);
    if (child_view->HitTestPoint(point_in_child_coords))
      return child_view->GetNativeViewAccessible();
  }

  // If it's not inside any of our children, it's inside this view.
  return GetNativeObject();
}

gfx::NativeViewAccessible ViewAXPlatformNodeDelegate::GetFocus() {
  gfx::NativeViewAccessible focus_override =
      ui::AXPlatformNode::GetPopupFocusOverride();
  if (focus_override)
    return focus_override;

  FocusManager* focus_manager = view()->GetFocusManager();
  View* focused_view =
      focus_manager ? focus_manager->GetFocusedView() : nullptr;

  return focused_view ? focused_view->GetNativeViewAccessible() : nullptr;
}

ui::AXPlatformNode* ViewAXPlatformNodeDelegate::GetFromNodeID(int32_t id) {
  return PlatformNodeFromNodeID(id);
}

bool ViewAXPlatformNodeDelegate::AccessibilityPerformAction(
    const ui::AXActionData& data) {
  return view()->HandleAccessibleAction(data);
}

bool ViewAXPlatformNodeDelegate::ShouldIgnoreHoveredStateForTesting() {
  return false;
}

bool ViewAXPlatformNodeDelegate::IsOffscreen() const {
  // TODO: need to implement.
  return false;
}

const ui::AXUniqueId& ViewAXPlatformNodeDelegate::GetUniqueId() const {
  return ViewAccessibility::GetUniqueId();
}

void ViewAXPlatformNodeDelegate::PopulateChildWidgetVector(
    std::vector<Widget*>* result_child_widgets,
    bool* is_tab_modal_showing) {
  // Only attach child widgets to the root view.
  Widget* widget = view()->GetWidget();
  // Note that during window close, a Widget may exist in a state where it has
  // no NativeView, but hasn't yet torn down its view hierarchy.
  if (!widget || !widget->GetNativeView() || widget->GetRootView() != view()) {
    *is_tab_modal_showing = false;
    return;
  }

  const views::FocusManager* focus_manager = view()->GetFocusManager();
  const views::View* focused_view =
      focus_manager ? focus_manager->GetFocusedView() : nullptr;

  std::set<Widget*> child_widgets;
  Widget::GetAllOwnedWidgets(widget->GetNativeView(), &child_widgets);
  for (auto iter = child_widgets.begin(); iter != child_widgets.end(); ++iter) {
    Widget* child_widget = *iter;
    DCHECK_NE(widget, child_widget);

    if (!child_widget->IsVisible())
      continue;

    if (widget->GetNativeWindowProperty(kWidgetNativeViewHostKey))
      continue;

    // Focused child widgets should take the place of the web page they cover in
    // the accessibility tree.
    if (ViewAccessibilityUtils::IsFocusedChildWidget(child_widget,
                                                     focused_view)) {
      result_child_widgets->clear();
      result_child_widgets->push_back(child_widget);
      *is_tab_modal_showing = true;
      return;
    }

    result_child_widgets->push_back(child_widget);
  }
  *is_tab_modal_showing = false;
}

}  // namespace views
