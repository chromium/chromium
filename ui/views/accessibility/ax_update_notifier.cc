// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/accessibility/ax_update_notifier.h"

#include "base/no_destructor.h"
#include "base/observer_list.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/views/accessibility/ax_update_observer.h"
#include "ui/views/accessibility/ax_virtual_view.h"
#include "ui/views/accessibility/tree/widget_ax_manager.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace views {

AXUpdateNotifier::AXUpdateNotifier() = default;

AXUpdateNotifier::~AXUpdateNotifier() = default;

// static
AXUpdateNotifier* AXUpdateNotifier::Get() {
  static base::NoDestructor<AXUpdateNotifier> instance;
  return instance.get();
}

void AXUpdateNotifier::AddObserver(AXUpdateObserver* observer) {
  observers_.AddObserver(observer);
}

void AXUpdateNotifier::RemoveObserver(AXUpdateObserver* observer) {
  observers_.RemoveObserver(observer);
}

void AXUpdateNotifier::NotifyViewEvent(views::View* view,
                                       ax::mojom::Event event_type) {
  CHECK(view);
  observers_.Notify(&AXUpdateObserver::OnViewEvent, view, event_type);

  // Directly notify the WidgetAXManager of the view's widget, avoiding the
  // need to register all WidgetAXManagers as observers and broadcasting events
  // to unrelated widgets.
  auto* widget = view->GetWidget();
  if (::features::IsAccessibilityTreeForViewsEnabled() && widget) {
    widget->ax_manager()->OnEvent(view->GetViewAccessibility(), event_type);
  }
}

void AXUpdateNotifier::NotifyVirtualViewEvent(
    views::AXVirtualView* virtual_view,
    ax::mojom::Event event_type) {
  CHECK(virtual_view);
  observers_.Notify(&AXUpdateObserver::OnVirtualViewEvent, virtual_view,
                    event_type);

  // Directly notify the WidgetAXManager of the virtual view's widget, avoiding
  // the need to register all WidgetAXManagers as observers and broadcasting
  // events to unrelated widgets.
  auto* widget = virtual_view->GetWidget();
  if (::features::IsAccessibilityTreeForViewsEnabled() && widget) {
    widget->ax_manager()->OnEvent(*virtual_view, event_type);
  }
}

void AXUpdateNotifier::NotifyViewDataChanged(views::View* view) {
  CHECK(view);
  if (!::features::IsViewsAccessibilitySerializeOnDataChangeEnabled()) {
    return;
  }

  observers_.Notify(&AXUpdateObserver::OnDataChanged, view);

  // Directly notify the WidgetAXManager of the view's widget, avoiding the need
  // to register all WidgetAXManagers as observers and broadcasting updates to
  // unrelated widgets.
  auto* widget = view->GetWidget();
  if (::features::IsAccessibilityTreeForViewsEnabled() && widget) {
    widget->ax_manager()->OnDataChanged(view->GetViewAccessibility());
  }
}

void AXUpdateNotifier::NotifyVirtualViewDataChanged(
    views::AXVirtualView* virtual_view) {
  CHECK(virtual_view);
  if (!::features::IsViewsAccessibilitySerializeOnDataChangeEnabled()) {
    return;
  }

  observers_.Notify(&AXUpdateObserver::OnVirtualViewDataChanged, virtual_view);

  // Directly notify the WidgetAXManager of the view's widget, avoiding the need
  // to register all WidgetAXManagers as observers and broadcasting updates to
  // unrelated widgets.
  auto* widget = virtual_view->GetWidget();
  if (::features::IsAccessibilityTreeForViewsEnabled() && widget) {
    widget->ax_manager()->OnDataChanged(*virtual_view);
  }
}

void AXUpdateNotifier::NotifyChildAdded(views::ViewAccessibility* child,
                                        views::ViewAccessibility* parent) {
  CHECK(child);
  CHECK(parent);
  observers_.Notify(&AXUpdateObserver::OnChildAdded, child, parent);

  // Directly notify the WidgetAXManager of the parent's widget, avoiding the
  // need to register all WidgetAXManagers as observers and broadcasting updates
  // to unrelated widgets.
  auto* widget = parent->GetWidget();
  if (::features::IsAccessibilityTreeForViewsEnabled() && widget) {
    widget->ax_manager()->OnChildAdded(*child, *parent);
  }
}

void AXUpdateNotifier::NotifyChildRemoved(views::ViewAccessibility* child,
                                          views::ViewAccessibility* parent) {
  CHECK(child);
  CHECK(parent);
  observers_.Notify(&AXUpdateObserver::OnChildRemoved, child, parent);

  // Directly notify the WidgetAXManager of the parent's widget, avoiding the
  // need to register all WidgetAXManagers as observers and broadcasting updates
  // to unrelated widgets.
  auto* widget = parent->GetWidget();
  if (::features::IsAccessibilityTreeForViewsEnabled() && widget) {
    widget->ax_manager()->OnChildRemoved(*child, *parent);
  }
}

}  // namespace views
