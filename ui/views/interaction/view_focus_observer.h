// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_INTERACTION_VIEW_FOCUS_OBSERVER_H_
#define UI_VIEWS_INTERACTION_VIEW_FOCUS_OBSERVER_H_

#include "base/scoped_observation.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/state_observer.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget_observer.h"

namespace views::test {

// Base class for focus observers. Observes focus within a specific widget's
// hierarchy (including other widgets that share the same focus manager).
//
// The value of this observer becomes null/default when the focus manager goes
// away.
class ViewFocusObserverBase : FocusChangeListener, WidgetObserver {
 public:
  explicit ViewFocusObserverBase(Widget* widget);
  ViewFocusObserverBase(const ViewFocusObserverBase&) = delete;
  void operator=(const ViewFocusObserverBase&) = delete;
  ~ViewFocusObserverBase() override;

 protected:
  virtual void OnFocusChanged(View* new_focused_view) = 0;
  View* GetFocusedView() const;

 private:
  // FocusChangeListener:
  void OnWillChangeFocus(View*, View*) override;
  void OnDidChangeFocus(View*, View*) override;

  // WidgetObserver:
  void OnWidgetDestroying(Widget*) override;

  raw_ptr<FocusManager> focus_manager_ = nullptr;
  base::ScopedObservation<Widget, WidgetObserver> widget_observation_{this};
};

// Observer that tracks focus within a given widget and other surfaces that
// share a focus manager by the focused view.
class ViewFocusObserverByView final : public ViewFocusObserverBase,
                                      public ui::test::StateObserver<View*> {
 public:
  explicit ViewFocusObserverByView(Widget* widget);
  ~ViewFocusObserverByView() override = default;

  View* GetStateObserverInitialState() const override;

 protected:
  void OnFocusChanged(View* new_focused_view) override;
};

// Observer that tracks focus within a given widget and other surfaces that
// share a focus manager by the ElementIdentifier of the focused view; the
// identifier will be null if there is no focused view or if the focused view
// has no identifier.
class ViewFocusObserverByIdentifier final
    : public ViewFocusObserverBase,
      public ui::test::StateObserver<ui::ElementIdentifier> {
 public:
  explicit ViewFocusObserverByIdentifier(Widget* widget);
  ~ViewFocusObserverByIdentifier() override = default;

  ui::ElementIdentifier GetStateObserverInitialState() const override;

 protected:
  void OnFocusChanged(View* new_focused_view) override;
};

// Convenient identifiers for the two types of state observers defined above.
// You can use these if you only need one focus observer fo the given type.
DECLARE_STATE_IDENTIFIER_VALUE(ViewFocusObserverByView, kCurrentFocusedView);
DECLARE_STATE_IDENTIFIER_VALUE(ViewFocusObserverByIdentifier,
                               kCurrentFocusedViewId);

}  // namespace views::test

#endif  // UI_VIEWS_INTERACTION_VIEW_FOCUS_OBSERVER_H_
