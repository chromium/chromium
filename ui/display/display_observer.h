// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_DISPLAY_OBSERVER_H_
#define UI_DISPLAY_DISPLAY_OBSERVER_H_

#include <stdint.h>

#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/observer_list_types.h"
#include "ui/display/display_export.h"

namespace display {
class Display;
enum class TabletState;

using Displays = std::vector<Display>;

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
    DISPLAY_METRIC_REFRESH_RATE = 1 << 7,
    DISPLAY_METRIC_INTERLACED = 1 << 8,
    DISPLAY_METRIC_LABEL = 1 << 9,
    DISPLAY_METRIC_VRR = 1 << 10,
    DISPLAY_METRIC_DETECTED = 1 << 11,
  };

  // Called when |new_display| has been added.
  virtual void OnDisplayAdded(const Display& new_display) {}

  // Called before displays have been removed.
  virtual void OnWillRemoveDisplays(const Displays& removed_displays) {}

  // Called *after* `removed_displays` have been removed. Not called per
  // display.
  virtual void OnDisplaysRemoved(const Displays& removed_displays) {}

  // Called when the metrics of a display change.
  // |changed_metrics| is a bitmask of DisplayMetric types indicating which
  // metrics have changed. Eg; if mirroring changes (either from true to false,
  // or false to true), than the DISPLAY_METRIC_MIRROR_STATE bit is set in
  // changed_metrics.
  virtual void OnDisplayMetricsChanged(const Display& display,
                                       uint32_t changed_metrics) {}

  // Called when the (platform-specific) workspace ID changes to
  // |new_workspace|.
  virtual void OnCurrentWorkspaceChanged(const std::string& new_workspace) {}

  // Called when display changes between conventional and tablet mode.
  virtual void OnDisplayTabletStateChanged(TabletState state) {}

 protected:
  ~DisplayObserver() override;
};

// Caller must ensure the lifetime of `observer` outlives ScopedDisplayObserver
// and ScopedOptionalDisplayObserver.  The "Optional" version does not care
// whether there is a display::Screen::GetScreen() to observe or not and will
// silently noop when there is not.  The non-optional ScopedDisplayObserver
// will CHECK that display::Screen::GetScreen() exists on construction to
// receive events from.
class DISPLAY_EXPORT ScopedOptionalDisplayObserver {
 public:
  explicit ScopedOptionalDisplayObserver(DisplayObserver* observer);
  ~ScopedOptionalDisplayObserver();

 private:
  raw_ptr<DisplayObserver> observer_ = nullptr;
};

class DISPLAY_EXPORT ScopedDisplayObserver
    : public ScopedOptionalDisplayObserver {
 public:
  explicit ScopedDisplayObserver(DisplayObserver* observer);
};

}  // namespace display

#endif  // UI_DISPLAY_DISPLAY_OBSERVER_H_
