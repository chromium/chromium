// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_INPUT_PROTECTION_OCCLUDED_WIDGET_INPUT_PROTECTOR_H_
#define UI_VIEWS_INPUT_PROTECTION_OCCLUDED_WIDGET_INPUT_PROTECTOR_H_

#include <map>
#include <set>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/singleton.h"
#include "base/scoped_multi_source_observation.h"
#include "base/time/time.h"
#include "base/types/pass_key.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/views_export.h"
#include "ui/views/widget/widget_observer.h"

namespace ui {
class Event;
}

namespace views {

class View;
class Widget;

namespace test {
class OccludedWidgetInputProtectorTestBase;
}

// Singleton to support always-on-top input protection for Widgets.
class VIEWS_EXPORT OccludedWidgetInputProtector : public views::WidgetObserver {
 public:
  // Represents a screen area recently occluded by an always-on-top widget.
  // These records protect against programmatic "pop-away" or "pop-under"
  // attacks (e.g., hiding or moving an always-on-top widget). Records expire
  // after the double-click interval.
  struct HistoricalOcclusion {
    gfx::Rect bounds;
    base::TimeTicks timestamp;
  };

  static OccludedWidgetInputProtector* GetInstance();

  OccludedWidgetInputProtector(const OccludedWidgetInputProtector&) = delete;
  OccludedWidgetInputProtector& operator=(const OccludedWidgetInputProtector&) =
      delete;

  // Returns true if `event` should be blocked due to occlusion by an
  // always-on-top widget. For located events, this checks if the event's
  // location is, or has recently been, occluded.
  //
  // TODO(crbug.com/495850608): Consult with UX to determine the best heuristic
  // for non-located event occlusion.
  bool ShouldBlockEvent(const ui::Event& event, const View& target_view) const;

  // Updates the tracking state of the protector for `widget`. The protector
  // will decide whether to track or stop tracking the widget based on its
  // current state (Z-order level).
  //
  // Restricted to Widget via PassKey.
  void UpdateTracking(base::PassKey<views::Widget> pass_key,
                      views::Widget* widget);

  // views::WidgetObserver:
  void OnWidgetVisibilityChanged(views::Widget* widget, bool visible) override;
  void OnWidgetDestroying(views::Widget* widget) override;
  void OnWidgetBoundsChanged(views::Widget* widget,
                             const gfx::Rect& new_bounds) override;
  void OnWidgetUserResizeStarted(views::Widget* widget) override;
  void OnWidgetUserResizeEnded(views::Widget* widget) override;

  // Clears all tracked widgets and historical occlusion records.
  void ClearForTesting();

 private:
  friend struct base::DefaultSingletonTraits<OccludedWidgetInputProtector>;
  friend class test::OccludedWidgetInputProtectorTestBase;

  OccludedWidgetInputProtector();
  ~OccludedWidgetInputProtector() override;

  // Internal implementation of `UpdateTracking`.
  void UpdateTrackingImpl(views::Widget* widget);

  // Registers `widget` for always-on-top monitoring.
  void Register(views::Widget* widget);

  // Unregisters `widget` from always-on-top monitoring.
  void Unregister(views::Widget* widget);

  // Stops tracking `widget` and records historical occlusion if necessary.
  void StopTracking(views::Widget* widget);

  // Returns true if the widget is being dragged or resized by the user.
  bool IsManuallyManipulated(views::Widget* widget) const;

  // Records a screen area as recently occluded.
  void RecordHistoricalOcclusion(const gfx::Rect& bounds);

  // Prunes expired cached `HistoricalOcclusion` occlusion records if any exist.
  //
  // Permitted during const operations because this method only updates mutable
  // cached data (`occlusion_history_`).
  void PruneCachedOcclusionHistory() const;

  // Returns true if `record` is older than the double-click interval.
  bool IsRecordExpired(const HistoricalOcclusion& record) const;

  // Returns true if the `target` point is occluded by any tracked always-on-top
  // widgets or historical occlusion records.
  bool CheckPointOcclusion(const gfx::Point& target) const;

  // Returns true if the `target` rects are occluded by any tracked
  // always-on-top widgets or historical occlusion records. If
  // `check_intersection` is true, checks for partial occlusion (intersection);
  // otherwise, checks for full occlusion (containment).
  bool CheckRectsOcclusion(const std::vector<gfx::Rect>& target,
                           bool check_intersection = true) const;

  // The set of always-on-top widgets currently being tracked, mapped to their
  // last known non-decorated client area bounds in screen coordinates.
  std::map<views::Widget*, gfx::Rect> always_on_top_widgets_;

  // Always-on-top widgets currently being resized by the user.
  std::set<raw_ptr<views::Widget>> resizing_widgets_;

  // A history of screen rects that have recently been covered by always-on-top
  // windows, stored in chronological order (oldest first).
  //
  // Marked mutable because it acts as a cache of recently occluded areas and
  // is lazily pruned during const operations (specifically `ShouldBlockEvent`).
  mutable base::circular_deque<HistoricalOcclusion> occlusion_history_;

  base::ScopedMultiSourceObservation<views::Widget, views::WidgetObserver>
      widget_observations_{this};
};

}  // namespace views

#endif  // UI_VIEWS_INPUT_PROTECTION_OCCLUDED_WIDGET_INPUT_PROTECTOR_H_
