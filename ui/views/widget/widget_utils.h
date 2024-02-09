// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_WIDGET_UTILS_H_
#define UI_VIEWS_WIDGET_WIDGET_UTILS_H_

#include "base/functional/callback.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/views_export.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace views {
class Widget;

class VIEWS_EXPORT WidgetOpenTimer : public WidgetObserver {
 public:
  using Callback = base::RepeatingCallback<void(base::TimeDelta)>;

  explicit WidgetOpenTimer(Callback callback);
  WidgetOpenTimer(const WidgetOpenTimer&) = delete;
  const WidgetOpenTimer& operator=(const WidgetOpenTimer&) = delete;
  ~WidgetOpenTimer() override;

  // WidgetObserver:
  void OnWidgetDestroying(Widget* widget) override;

  // Called to start the |open_timer_|.
  void Reset(Widget* widget);

 private:
  // Callback run when the passed in Widget is destroyed.
  Callback callback_;

  // Time the bubble has been open. Used for UMA metrics collection.
  std::optional<base::ElapsedTimer> open_timer_;

  base::ScopedObservation<Widget, WidgetObserver> observed_widget_{this};
};

// Returns the root window for |widget|.  On non-Aura, this is equivalent to
// widget->GetNativeWindow().
VIEWS_EXPORT gfx::NativeWindow GetRootWindow(const Widget* widget);

}  // namespace views

#endif  // UI_VIEWS_WIDGET_WIDGET_UTILS_H_
