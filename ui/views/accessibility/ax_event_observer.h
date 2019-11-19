// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ACCESSIBILITY_AX_EVENT_OBSERVER_H_
#define UI_VIEWS_ACCESSIBILITY_AX_EVENT_OBSERVER_H_

#include "base/observer_list_types.h"
#include "ui/accessibility/ax_enums.mojom-forward.h"
#include "ui/views/views_export.h"

namespace views {

class View;

// AXEventObserver is notified for accessibility events on all views.
class VIEWS_EXPORT AXEventObserver : public base::CheckedObserver {
 public:
  virtual void OnViewEvent(views::View* view, ax::mojom::Event event_type) = 0;

 protected:
  AXEventObserver();
  ~AXEventObserver() override;
};

}  // namespace views

#endif  // UI_VIEWS_ACCESSIBILITY_AX_EVENT_OBSERVER_H_
