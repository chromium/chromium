// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/event_monitor_aura.h"

#include "base/logging.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/events/event_observer.h"
#include "ui/events/event_target.h"

namespace views {

// static
std::unique_ptr<EventMonitor> EventMonitor::CreateApplicationMonitor(
    ui::EventObserver* event_observer,
    gfx::NativeWindow context,
    const std::set<ui::EventType>& types) {
  aura::Env* env = context->env();
  return std::make_unique<EventMonitorAura>(env, event_observer, env, types);
}

// static
std::unique_ptr<EventMonitor> EventMonitor::CreateWindowMonitor(
    ui::EventObserver* event_observer,
    gfx::NativeWindow target_window,
    const std::set<ui::EventType>& types) {
  return std::make_unique<EventMonitorAura>(
      target_window->env(), event_observer, target_window, types);
}

EventMonitorAura::EventMonitorAura(aura::Env* env,
                                   ui::EventObserver* event_observer,
                                   ui::EventTarget* event_target,
                                   const std::set<ui::EventType>& types)
    : env_(env), event_observer_(event_observer), event_target_(event_target) {
  DCHECK(env_);
  DCHECK(event_observer_);
  DCHECK(event_target_);
  env_->AddEventObserver(event_observer_, event_target, types);
}

EventMonitorAura::~EventMonitorAura() {
  env_->RemoveEventObserver(event_observer_);
}

gfx::Point EventMonitorAura::GetLastMouseLocation() {
  return env_->last_mouse_location();
}

}  // namespace views
