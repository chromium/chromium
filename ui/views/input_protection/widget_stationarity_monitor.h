// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_INPUT_PROTECTION_WIDGET_STATIONARITY_MONITOR_H_
#define UI_VIEWS_INPUT_PROTECTION_WIDGET_STATIONARITY_MONITOR_H_

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/no_destructor.h"
#include "base/scoped_multi_source_observation.h"
#include "base/types/pass_key.h"
#include "ui/views/views_export.h"
#include "ui/views/widget/widget_observer.h"

namespace gfx {
class Rect;
}

namespace views {

class InputEventActivationProtector;
class Widget;

// A singleton that notifies subscribers whenever a registered Widget changes
// bounds or closes. This is used by `InputEventActivationProtector` to suppress
// unintended user input immediately following such changes.
class VIEWS_EXPORT WidgetStationarityMonitor : public WidgetObserver {
 public:
  using StationarityChangedCallbackList = base::RepeatingClosureList;

  WidgetStationarityMonitor(const WidgetStationarityMonitor&) = delete;
  WidgetStationarityMonitor& operator=(const WidgetStationarityMonitor&) =
      delete;

  ~WidgetStationarityMonitor() override;

  static WidgetStationarityMonitor& GetInstance();

  // Registers `widget` to be tracked. Subscribers will be notified whenever
  // this widget changes bounds or closes.
  void TrackWidget(Widget& widget);

  // Registers `callback` to be called whenever any tracked widget changes
  // bounds or closes.
  //
  // The callback does not receive which widget changed. An occluding widget
  // may move or close to reveal content underneath, so subscribers need to
  // trigger when any tracked widget changes.
  //
  // Restricted to `InputEventActivationProtector` via `PassKey` to prevent the
  // introduction of Use-After-Free (UAF) vulnerabilities during widget
  // destruction (`OnWidgetDestroying`). Because callbacks run during widget
  // destruction, subscribers must not query or mutate widgets.
  //
  // TODO(crbug.com/467460499): Evaluate having callers query the most recent
  // widget change timestamp instead of firing callbacks, or turning this into a
  // dedicated policy, to avoid Use-After-Free risks during widget destruction.
  base::CallbackListSubscription RegisterStationarityChangedCallback(
      base::PassKey<InputEventActivationProtector>,
      StationarityChangedCallbackList::CallbackType callback);

  base::CallbackListSubscription RegisterStationarityChangedCallbackForTesting(
      StationarityChangedCallbackList::CallbackType callback);

  // WidgetObserver:
  void OnWidgetBoundsChanged(Widget* widget,
                             const gfx::Rect& new_bounds) override;
  void OnWidgetDestroying(Widget* widget) override;

 private:
  friend class base::NoDestructor<WidgetStationarityMonitor>;

  WidgetStationarityMonitor();

  void NotifyStationaryStateChanged();

  StationarityChangedCallbackList stationarity_changed_callbacks_;
  base::ScopedMultiSourceObservation<Widget, WidgetObserver>
      widget_observations_{this};
};

}  // namespace views

#endif  // UI_VIEWS_INPUT_PROTECTION_WIDGET_STATIONARITY_MONITOR_H_
