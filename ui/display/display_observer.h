// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_DISPLAY_OBSERVER_H_
#define UI_DISPLAY_DISPLAY_OBSERVER_H_

#include <stdint.h>

#include "base/observer_list_types.h"
#include "ui/display/display_export.h"

namespace display {
class Display;

// Observers for display configuration changes.
class DISPLAY_EXPORT DisplayObserver : public base::CheckedObserver {
 public:
  enum DisplayMetric {
    DISPLAY_METRIC_NONE = 0,
    DISPLAY_METRIC_BOUNDS = 1 << 0,
    DISPLAY_METRIC_WORK_AREA = 1 << 1,
    DISPLAY_METRIC_DEVICE_SCALE_FACTOR = 1 << 2,
    DISPLAY_METRIC_ROTATION = 1 << 3,
    DISPLAY_METRIC_PRIMARY = 1 << 4,
    DISPLAY_METRIC_MIRROR_STATE = 1 << 5,
    DISPLAY_METRIC_COLOR_SPACE = 1 << 6,
  };

  // This may be called before other methods to signal changes are about to
  // happen. Not all classes that support DisplayObserver call this.
  virtual void OnWillProcessDisplayChanges();

  // Called after OnWillProcessDisplayChanges() to indicate display changes have
  // completed. Not all classes that support DisplayObserver call this.
  virtual void OnDidProcessDisplayChanges();

  // Called when |new_display| has been added.
  virtual void OnDisplayAdded(const Display& new_display);

  // Called when |old_display| has been removed.
  virtual void OnDisplayRemoved(const Display& old_display);

  // Called when the metrics of a display change.
  // |changed_metrics| is a bitmask of DisplayMatric types indicating which
  // metrics have changed. Eg; if mirroring changes (either from true to false,
  // or false to true), than the DISPLAY_METRIC_MIRROR_STATE bit is set in
  // changed_metrics.
  virtual void OnDisplayMetricsChanged(const Display& display,
                                       uint32_t changed_metrics);

 protected:
  ~DisplayObserver() override;
};

}  // namespace display

#endif  // UI_DISPLAY_DISPLAY_OBSERVER_H_
