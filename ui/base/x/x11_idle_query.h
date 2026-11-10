// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_X_X11_IDLE_QUERY_H_
#define UI_BASE_X_X11_IDLE_QUERY_H_

#include <optional>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/event.h"
#include "ui/gfx/x/screensaver.h"
#include "ui/gfx/x/sync.h"

namespace ui {

// Tracks user idle time asynchronously on X11 using XSync IDLETIME alarms, with
// an asynchronous MIT-SCREEN-SAVER QueryInfo fallback when IDLETIME is
// unavailable, so querying idle time never blocks the UI thread waiting for an
// X server reply.
class COMPONENT_EXPORT(UI_BASE_X) IdleQueryX11 : public x11::EventObserver {
 public:
  explicit IdleQueryX11(x11::Connection* connection = x11::Connection::Get());

  IdleQueryX11(const IdleQueryX11&) = delete;
  IdleQueryX11& operator=(const IdleQueryX11&) = delete;

  ~IdleQueryX11() override;

  base::TimeDelta IdleTime();

 private:
  enum class Mode {
    kInitializing,
    kSyncAlarms,
    kFallbackScreenSaver,
    kUnavailable,
  };

  // x11::EventObserver:
  void OnEvent(const x11::Event& event) override;

  void OnListSystemCounters(x11::Sync::ListSystemCountersResponse response);
  void ArmIdleAlarm();
  void ArmResetAlarm();
  void RequestFallbackIdleTime();
  void OnQueryInfoResponse(x11::ScreenSaver::QueryInfoResponse response);

  const raw_ptr<x11::Connection> connection_;
  Mode mode_ = Mode::kInitializing;
  bool observing_events_ = false;

  // XSync IDLETIME alarm state.
  x11::Sync::Counter idle_counter_{};
  x11::Sync::Alarm idle_alarm_{};
  x11::Sync::Alarm reset_alarm_{};
  std::optional<base::TimeTicks> idle_start_ticks_;

  // Fallback asynchronous ScreenSaver::QueryInfo state used only if the X
  // server lacks an XSync IDLETIME counter.
  bool query_in_flight_ = false;
  base::TimeTicks last_query_ticks_;
  base::TimeDelta cached_idle_time_;

  base::WeakPtrFactory<IdleQueryX11> weak_ptr_factory_{this};
};

}  // namespace ui

#endif  // UI_BASE_X_X11_IDLE_QUERY_H_
