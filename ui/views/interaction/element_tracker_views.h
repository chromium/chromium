// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_INTERACTION_ELEMENT_TRACKER_VIEWS_H_
#define UI_VIEWS_INTERACTION_ELEMENT_TRACKER_VIEWS_H_

#include <map>
#include <memory>
#include <vector>

#include "base/scoped_multi_source_observation.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/views/views_export.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace views {

class View;

// Wraps a View in an ui::ElementTrackerElement.
class VIEWS_EXPORT ElementTrackerElementViews
    : public ui::ElementTrackerElement {
 public:
  ElementTrackerElementViews(View* view,
                             ui::ElementIdentifier identifier,
                             ui::ElementContext context);
  ~ElementTrackerElementViews() override;

  View* view() { return view_; }
  const View* view() const { return view_; }

  DECLARE_ELEMENT_TRACKER_METADATA()

 private:
  View* const view_;
};

// Manages ElementTrackerElements associated with View objects.
class VIEWS_EXPORT ElementTrackerViews : private WidgetObserver {
 public:
  using ViewList = std::vector<View*>;

  // Gets the global instance of the tracker for Views.
  static ElementTrackerViews* GetInstance();

  // Returns the context associated with a particular View. The context will be
  // the same across all Views associated with a root Widget (such as an
  // application window).
  static ui::ElementContext GetContextForView(View* view);

  // Called by View after the kUniqueElementKey property is set.
  void RegisterView(ui::ElementIdentifier element_id, View* view);

  // Called by View if the kUniqueElementKey property is changed from a non-null
  // value.
  void UnregisterView(ui::ElementIdentifier element_id, View* view);

  // Called by a View when the user activates it (clicks a button, selects a
  // menu item, etc.)
  void NotifyViewActivated(ui::ElementIdentifier element_id, View* view);

 private:
  friend class base::NoDestructor<ElementTrackerViews>;
  class ElementDataViews;

  ElementTrackerViews();
  ~ElementTrackerViews() override;

  // WidgetObserver:
  void OnWidgetVisibilityChanged(Widget* widget, bool visible) override;
  void OnWidgetDestroying(Widget* widget) override;

  // We do not get notified at the View level if a view's widget has not yet
  // been shown. We need this notification to know when the view is actually
  // visible to the user. So if a view is added to the trakcer or is added to
  // a widget, and its widget is not visible, we watch it until it is (or it is
  // destroyed).
  void MaybeObserveWidget(Widget* widget);

  std::map<ui::ElementIdentifier, std::unique_ptr<ElementDataViews>>
      element_data_;
  base::ScopedMultiSourceObservation<Widget, WidgetObserver> widget_observer_{
      this};
};

}  // namespace views

#endif  // UI_VIEWS_INTERACTION_ELEMENT_TRACKER_VIEWS_H_
