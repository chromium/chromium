// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/accessibility/ax_update_notifier.h"

#include "base/no_destructor.h"
#include "base/observer_list.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/views/accessibility/ax_update_observer.h"

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
  observers_.Notify(&AXUpdateObserver::OnViewEvent, view, event_type);
}

void AXUpdateNotifier::NotifyVirtualViewEvent(
    views::AXVirtualView* virtual_view,
    ax::mojom::Event event_type) {
  observers_.Notify(&AXUpdateObserver::OnVirtualViewEvent, virtual_view,
                    event_type);
}

void AXUpdateNotifier::NotifyViewDataChanged(views::View* view) {
#if BUILDFLAG(IS_CHROMEOS)
  if (::features::IsViewsAccessibilitySerializeOnDataChangeEnabled()) {
    observers_.Notify(&AXUpdateObserver::OnDataChanged, view);
  }
#endif
}

void AXUpdateNotifier::NotifyVirtualViewDataChanged(
    views::AXVirtualView* virtual_view) {
#if BUILDFLAG(IS_CHROMEOS)
  if (::features::IsViewsAccessibilitySerializeOnDataChangeEnabled()) {
    observers_.Notify(&AXUpdateObserver::OnVirtualViewDataChanged,
                      virtual_view);
  }
#endif
}

}  // namespace views
