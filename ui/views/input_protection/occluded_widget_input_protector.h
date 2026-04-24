// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_INPUT_PROTECTION_OCCLUDED_WIDGET_INPUT_PROTECTOR_H_
#define UI_VIEWS_INPUT_PROTECTION_OCCLUDED_WIDGET_INPUT_PROTECTOR_H_

#include <set>

#include "base/memory/singleton.h"
#include "base/types/pass_key.h"
#include "ui/views/views_export.h"
#include "ui/views/widget/widget_observer.h"

namespace ui {
class Event;
}

namespace views {

class View;
class Widget;

// Singleton to support always-on-top input protection for Widgets.
class VIEWS_EXPORT OccludedWidgetInputProtector : public views::WidgetObserver {
 public:
  static OccludedWidgetInputProtector* GetInstance();

  OccludedWidgetInputProtector(const OccludedWidgetInputProtector&) = delete;
  OccludedWidgetInputProtector& operator=(const OccludedWidgetInputProtector&) =
      delete;

  // Returns true if `event` should be blocked due to occlusion by an
  // always-on-top widget. For located events, this checks if the event's
  // location is occluded. For non-located events, it checks if `target_view` is
  // significantly occluded.
  //
  // TODO(crbug.com/495850608): Consult with UX to determine the best heuristic
  // for non-located event occlusion.
  bool ShouldBlockEvent(const ui::Event& event, const View& target_view);

  // Updates the tracking state of the protector for `widget`. The protector
  // will decide whether to track or stop tracking the widget based on its
  // current state (Z-order level and visibility).
  //
  // Restricted to Widget via PassKey.
  void UpdateTracking(base::PassKey<views::Widget> pass_key,
                      views::Widget* widget);

  // views::WidgetObserver:
  void OnWidgetVisibilityChanged(views::Widget* widget, bool visible) override;
  void OnWidgetDestroying(views::Widget* widget) override;

  const std::set<views::Widget*>& always_on_top_widgets_for_testing() const {
    return always_on_top_widgets_;
  }

 private:
  friend struct base::DefaultSingletonTraits<OccludedWidgetInputProtector>;

  OccludedWidgetInputProtector();
  ~OccludedWidgetInputProtector() override;

  // Internal implementation of `UpdateTracking`.
  void UpdateTrackingImpl(views::Widget* widget);

  // Registers `widget` for always-on-top monitoring.
  void Register(views::Widget* widget);

  // Unregisters `widget` from always-on-top monitoring.
  void Unregister(views::Widget* widget);

  // The set of always-on-top widgets that are currently visible.
  std::set<views::Widget*> always_on_top_widgets_;
};

}  // namespace views

#endif  // UI_VIEWS_INPUT_PROTECTION_OCCLUDED_WIDGET_INPUT_PROTECTOR_H_
