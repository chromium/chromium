// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/interaction/interactive_views_test_internal.h"

#include <compare>
#include <deque>
#include <memory>
#include <set>
#include <utility>
#include <variant>
#include <vector>

#include "base/containers/map_util.h"
#include "base/scoped_observation.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/framework_specific_implementation.h"
#include "ui/base/interaction/interaction_test_util.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/focus/widget_focus_manager.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/interaction/widget_focus_observer.h"
#include "ui/views/native_window_tracker.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ui/aura/test/aura_test_helper.h"
#endif

namespace views::test::internal {

namespace {

// Basic observer for low-level activation changes. Relays when a widget
// receives focus.
class NativeViewWidgetFocusSupplier : public WidgetFocusSupplier,
                                      public WidgetFocusChangeListener {
 public:
  NativeViewWidgetFocusSupplier() {
    observation_.Observe(WidgetFocusManager::GetInstance());
  }
  ~NativeViewWidgetFocusSupplier() override = default;

  DECLARE_FRAMEWORK_SPECIFIC_METADATA()

  void OnNativeFocusChanged(gfx::NativeView focused_now) override {
    // TODO(dfried): There's an order-of-operations issue on some platforms
    // where focus transfers between two native views, and the blur for the old
    // view is received after the focus for the new view. This results in
    // `focused_now` being null rather than the currently-focused view.
    //
    // While it's slightly less correct, ignore blur events until this can be
    // fixed. In general, one would not expect windows not from the application
    // under test to become focused, so this will be a valid choice most of the
    // time.
    if (focused_now) {
      OnWidgetFocusChanged(focused_now);
    }
  }

 protected:
  Widget::Widgets GetAllWidgets() const override {
#if BUILDFLAG(IS_CHROMEOS)
    // On Ash, WidgetTest::GetAllWidgets() requires special test utils to be set
    // up that are incompatible with browser tests. If a test helper has been
    // set up, then use it, otherwise assume that the browser version will
    // handle fetching the widgets.
    Widget::Widgets result;
    if (aura::test::AuraTestHelper* const aura_test_helper =
            aura::test::AuraTestHelper::GetInstance()) {
      Widget::GetAllChildWidgets(aura_test_helper->GetContext(), &result);
    }
    return result;
#else
    return WidgetTest::GetAllWidgets();
#endif
  }

 private:
  base::ScopedObservation<WidgetFocusManager, WidgetFocusChangeListener>
      observation_{this};
};

DEFINE_FRAMEWORK_SPECIFIC_METADATA(NativeViewWidgetFocusSupplier)

// Takes a list of tracked `views` and massages them into a tree based on the
// views hierarchy, with widgets at the top level. (Widget parenting may be
// handled in a later update).
//
// Only views on the list are returned, which means that many container views
// may be omitted.
//
// Assumptions:
//  - All `TrackedElementViews` correspond to views in the view hierarchy, and
//    are attached to widgets.
//  - There are no circular parent-child relations in the views hierarchy.
InteractiveViewsTestPrivate::DebugTreeNodeViews::List DebugDumpViewHierarchy(
    std::vector<const TrackedElementViews*> views) {
  using Node = InteractiveViewsTestPrivate::DebugTreeNodeViews;
  using List = InteractiveViewsTestPrivate::DebugTreeNodeViews::List;

  // Need to know what views are participating in the hierarchy.
  std::map<const View*, const TrackedElementViews*> known_views;
  for (const auto& view : views) {
    known_views.emplace(view->view(), view);
  }

  // Keep separate track of widget nodes, which are fixed, and view nodes,
  // which can be in various places.
  std::map<const Widget*, Node> widget_nodes;
  std::map<const View*, Node*> view_nodes;
  for (const auto& view : views) {
    // It's possible this view was already added as the ancestor of another.
    if (view_nodes.contains(view->view())) {
      continue;
    }

    // Ensure a widget node exists.
    const Widget* const widget = view->view()->GetWidget();
    if (!widget_nodes.contains(widget)) {
      widget_nodes.emplace(widget, Node(widget));
    }

    // Add all known views in the current view's hierarchy, if they are not
    // already present.
    //
    // This ensure that upstream nodes are always created first, and nodes never
    // have to be moved after they are created.
    std::deque<const TrackedElementViews*> views_to_add{view};
    Node* to_add_to = nullptr;

    // Walk up the view hierarchy from the current view.
    for (const View* ancestor = view->view()->parent(); ancestor != nullptr;
         ancestor = ancestor->parent()) {
      // If there is already a node for an ancestor, add to that node.
      const auto it = view_nodes.find(ancestor);
      if (it != view_nodes.end()) {
        to_add_to = it->second;
        break;
      }

      // If there is no node but this is a known element, it must be added.
      if (known_views.contains(ancestor)) {
        views_to_add.push_front(known_views[ancestor]);
      }
    }

    // If no view in the hierarchy already has a node, add to the widget's node
    // instead.
    if (!to_add_to) {
      to_add_to = &widget_nodes[widget];
    }

    // Walk down the view hierarchy adding nodes from ancestor to descendant.
    for (const auto* view_to_add : views_to_add) {
      CHECK(view_to_add);
      CHECK(view_to_add->view());
      const auto add_child_result =
          to_add_to->children.emplace(view_to_add->view(), view_to_add);
      CHECK(add_child_result.second);
      // This is safe since we just verified we inserted the value.
      Node* const ptr = const_cast<Node*>(&*add_child_result.first);
      const auto add_entry_result =
          view_nodes.emplace(view_to_add->view(), ptr);
      CHECK(add_entry_result.second);
      to_add_to = add_entry_result.first->second;
    }
  }

  // Only widget nodes are included in the final list, since all views should
  // belong to a widget.
  List top_level;
  for (auto& entry : widget_nodes) {
    top_level.emplace(std::move(entry.second));
  }
  return top_level;
}

}  // namespace

InteractiveViewsTestPrivate::DebugTreeNodeViews::DebugTreeNodeViews() = default;
InteractiveViewsTestPrivate::DebugTreeNodeViews::DebugTreeNodeViews(
    const View* view,
    const ui::TrackedElement* el)
    : impl(view), element(el), bounds(view->GetBoundsInScreen()) {}
InteractiveViewsTestPrivate::DebugTreeNodeViews::DebugTreeNodeViews(
    const Widget* widget)
    : impl(widget), bounds(widget->GetWindowBoundsInScreen()) {}
InteractiveViewsTestPrivate::DebugTreeNodeViews::DebugTreeNodeViews(
    DebugTreeNodeViews&&) noexcept = default;
InteractiveViewsTestPrivate::DebugTreeNodeViews&
InteractiveViewsTestPrivate::DebugTreeNodeViews::operator=(
    DebugTreeNodeViews&&) noexcept = default;
InteractiveViewsTestPrivate::DebugTreeNodeViews::~DebugTreeNodeViews() =
    default;

InteractiveViewsTestPrivate::DebugTreeNode
InteractiveViewsTestPrivate::DebugTreeNodeViews::ToNode(
    const InteractiveViewsTestPrivate& owner) const {
  InteractiveViewsTestPrivate::DebugTreeNode result;
  if (std::holds_alternative<const View*>(impl)) {
    result = owner.DebugDumpElement(element);
  } else {
    result =
        DebugTreeNode(owner.DebugDumpWidget(*std::get<const Widget*>(impl)));
  }
  for (auto& child : children) {
    result.children.emplace_back(child.ToNode(owner));
  }
  return result;
}

std::strong_ordering
InteractiveViewsTestPrivate::DebugTreeNodeViews::operator<=>(
    const DebugTreeNodeViews& other) const {
  auto result = bounds.x() <=> other.bounds.x();
  if (result != std::strong_ordering::equal) {
    return result;
  }
  result = bounds.y() <=> other.bounds.y();
  if (result != std::strong_ordering::equal) {
    return result;
  }
  return impl <=> other.impl;
}

// Caches the last-known native window associated with a context.
// Useful for executing ClickMouse() and ReleaseMouse() commands, as no target
// element is provided for those commands. A NativeWindowTracker is used to
// prevent using a cached value after the native window has been destroyed.
class InteractiveViewsTestPrivate::WindowHintCacheEntry {
 public:
  WindowHintCacheEntry() = default;
  ~WindowHintCacheEntry() = default;
  WindowHintCacheEntry(WindowHintCacheEntry&& other) = default;
  WindowHintCacheEntry& operator=(WindowHintCacheEntry&& other) = default;

  bool IsValid() const {
    return window_ && tracker_ && !tracker_->WasNativeWindowDestroyed();
  }

  gfx::NativeWindow GetWindow() const {
    return IsValid() ? window_ : gfx::NativeWindow();
  }

  void SetWindow(gfx::NativeWindow window) {
    if (window_ == window)
      return;
    window_ = window;
    tracker_ = window ? views::NativeWindowTracker::Create(window) : nullptr;
  }

 private:
  gfx::NativeWindow window_ = gfx::NativeWindow();
  std::unique_ptr<NativeWindowTracker> tracker_;
};

InteractiveViewsTestPrivate::InteractiveViewsTestPrivate(
    std::unique_ptr<ui::test::InteractionTestUtil> test_util)
    : InteractiveTestPrivate(std::move(test_util)) {}

InteractiveViewsTestPrivate::~InteractiveViewsTestPrivate() = default;

void InteractiveViewsTestPrivate::OnSequenceComplete() {
  if (mouse_util_) {
    mouse_util_->CancelAllGestures();
  }
  InteractiveTestPrivate::OnSequenceComplete();
}

void InteractiveViewsTestPrivate::OnSequenceAborted(
    const ui::InteractionSequence::AbortedData& data) {
  if (mouse_util_) {
    mouse_util_->CancelAllGestures();
  }
  InteractiveTestPrivate::OnSequenceAborted(data);
}

void InteractiveViewsTestPrivate::DoTestSetUp() {
  InteractiveTestPrivate::DoTestSetUp();
  // Frame should exist from set up to tear down, to prevent framework/system
  // listeners from receiving events outside of the test.
  widget_focus_supplier_frame_ = std::make_unique<WidgetFocusSupplierFrame>();
  widget_focus_suppliers().MaybeRegister<NativeViewWidgetFocusSupplier>();
}

void InteractiveViewsTestPrivate::DoTestTearDown() {
  // Avoid doing any widget focus tracking after the test completes.
  widget_focus_supplier_frame_.reset();
  InteractiveTestPrivate::DoTestTearDown();
}

gfx::NativeWindow InteractiveViewsTestPrivate::GetWindowHintFor(
    ui::TrackedElement* el) {
  // See if the native window can be extracted directly from the element.
  gfx::NativeWindow window = GetNativeWindowFromElement(el);

  // If not, see if the window can be extracted from the context (perhaps via
  // the cache).
  if (!window)
    window = GetNativeWindowFromContext(el->context());

  // If a window was found, then a cache entry may need to be inserted/updated.
  if (window) {
    // This is just a find if the entry already exists.
    auto result =
        window_hint_cache_.try_emplace(el->context(), WindowHintCacheEntry());
    // This is a no-op if this is already the cached window.
    result.first->second.SetWindow(window);
  }

  return window;
}

gfx::NativeWindow InteractiveViewsTestPrivate::GetNativeWindowFromElement(
    ui::TrackedElement* el) const {
  gfx::NativeWindow window = gfx::NativeWindow();
  if (el->IsA<TrackedElementViews>()) {
    // Most widgets have an associated native window.
    Widget* const widget = el->AsA<TrackedElementViews>()->view()->GetWidget();
    window = widget->GetNativeWindow();
    // Most of those that don't are sub-widgets that are hard-parented to
    // another widget.
    if (!window && widget->parent())
      window = widget->parent()->GetNativeWindow();
    // At worst case, fall back to the primary window.
    if (!window)
      window = widget->GetPrimaryWindowWidget()->GetNativeWindow();
  }
  return window;
}

gfx::NativeWindow InteractiveViewsTestPrivate::GetNativeWindowFromContext(
    ui::ElementContext context) const {
  // Used the cached value, if one exists.
  const auto it = window_hint_cache_.find(context);
  return it != window_hint_cache_.end() ? it->second.GetWindow()
                                        : gfx::NativeWindow();
}

std::string InteractiveViewsTestPrivate::DebugDumpWidget(
    const Widget& widget) const {
  std::string description = widget.GetName();
  return base::StringPrintf(
      "%s \"%s\" at %s", widget.GetClassName(), widget.GetName().c_str(),
      DebugDumpBounds(widget.GetWindowBoundsInScreen()).c_str());
}

InteractiveViewsTestPrivate::DebugTreeNode
InteractiveViewsTestPrivate::DebugDumpElement(
    const ui::TrackedElement* el) const {
  if (const auto* view = el->AsA<TrackedElementViews>()) {
    return DebugTreeNode(base::StringPrintf(
        "%s%s - %s at %s", (view->view()->HasFocus() ? "[FOCUSED] " : ""),
        view->view()->GetClassName(), el->identifier().GetName().c_str(),
        DebugDumpBounds(el->GetScreenBounds())));
  }
  return InteractiveTestPrivate::DebugDumpElement(el);
}

InteractiveViewsTestPrivate::DebugTreeNode
InteractiveViewsTestPrivate::DebugDumpContext(
    ui::ElementContext context) const {
  DebugTreeNode node(DebugDescribeContext(context));
  auto* const tracker = ui::ElementTracker::GetElementTracker();
  std::vector<const TrackedElementViews*> views;
  for (const auto* const element : tracker->GetAllElementsForTesting(context)) {
    if (const auto* const view_el = element->AsA<TrackedElementViews>()) {
      views.push_back(view_el);
    } else {
      node.children.emplace_back(DebugDumpElement(element));
    }
  }
  for (auto& view_node : DebugDumpViewHierarchy(views)) {
    node.children.emplace_back(view_node.ToNode(*this));
  }
  return node;
}

}  // namespace views::test::internal
