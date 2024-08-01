// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_THROBBER_H_
#define UI_VIEWS_CONTROLS_THROBBER_H_

#include "base/time/time.h"
#include "base/timer/timer.h"
#include "ui/views/view.h"

namespace views {

// Throbbers display an animation, usually used as a status indicator.

class VIEWS_EXPORT Throbber : public View {
  METADATA_HEADER(Throbber, View)

 public:
  explicit Throbber(int diameter = kDefaultDiameter);

  Throbber(const Throbber&) = delete;
  Throbber& operator=(const Throbber&) = delete;

  ~Throbber() override;

  // Start and stop the throbber animation.
  virtual void Start();
  virtual void Stop();

  // Gets/Sets checked. For SetChecked, stop spinning and, if
  // checked is true, display a checkmark.
  bool GetChecked() const;
  void SetChecked(bool checked);

  // Overridden from View:
  gfx::Size CalculatePreferredSize(
      const SizeBounds& /*available_size*/) const override;
  void OnPaint(gfx::Canvas* canvas) override;

  int GetDiameter() const { return diameter_; }

 protected:
  // Specifies whether the throbber is currently animating or not
  bool IsRunning() const;

  // The default diameter of a Throbber.
  static constexpr int kDefaultDiameter = 16;

 private:
  base::TimeTicks start_time_;  // Time when Start was called.
  base::RepeatingTimer timer_;  // Used to schedule Run calls.

  // Whether or not we should display a checkmark.
  bool checked_ = false;

  const int diameter_;
};

BEGIN_VIEW_BUILDER(VIEWS_EXPORT, Throbber, View)
VIEW_BUILDER_PROPERTY(bool, Checked)
END_VIEW_BUILDER

// A SmoothedThrobber is a throbber that is representing potentially short
// and nonoverlapping bursts of work.  SmoothedThrobber ignores small
// pauses in the work stops and starts, and only starts its throbber after
// a small amount of work time has passed.
class VIEWS_EXPORT SmoothedThrobber : public Throbber {
  METADATA_HEADER(SmoothedThrobber, Throbber)

 public:
  explicit SmoothedThrobber(int diameter = kDefaultDiameter);

  SmoothedThrobber(const SmoothedThrobber&) = delete;
  SmoothedThrobber& operator=(const SmoothedThrobber&) = delete;

  ~SmoothedThrobber() override;

  void Start() override;
  void Stop() override;

  base::TimeDelta GetStartDelay() const;
  void SetStartDelay(const base::TimeDelta& start_delay);

  base::TimeDelta GetStopDelay() const;
  void SetStopDelay(const base::TimeDelta& stop_delay);

 private:
  // Called when the startup-delay timer fires
  // This function starts the actual throbbing.
  void StartDelayOver();

  // Called when the shutdown-delay timer fires.
  // This function stops the actual throbbing.
  void StopDelayOver();

  // Delay after work starts before starting throbber.
  base::TimeDelta start_delay_;

  // Delay after work stops before stopping.
  base::TimeDelta stop_delay_;

  base::OneShotTimer start_timer_;
  base::OneShotTimer stop_timer_;
};

BEGIN_VIEW_BUILDER(VIEWS_EXPORT, SmoothedThrobber, Throbber)
VIEW_BUILDER_PROPERTY(const base::TimeDelta&, StartDelay)
VIEW_BUILDER_PROPERTY(const base::TimeDelta&, StopDelay)
END_VIEW_BUILDER

}  // namespace views

DEFINE_VIEW_BUILDER(VIEWS_EXPORT, Throbber)
DEFINE_VIEW_BUILDER(VIEWS_EXPORT, SmoothedThrobber)

#endif  // UI_VIEWS_CONTROLS_THROBBER_H_
