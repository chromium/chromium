// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/test/event_generator_delegate_aura.h"

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/test/default_event_generator_delegate.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/ime/input_method.h"
#include "ui/events/event_source.h"

namespace aura {
namespace test {
namespace {

const Window* WindowFromTarget(const ui::EventTarget* event_target) {
  return static_cast<const Window*>(event_target);
}
Window* WindowFromTarget(ui::EventTarget* event_target) {
  return static_cast<Window*>(event_target);
}

}  // namespace

// static
std::unique_ptr<ui::test::EventGeneratorDelegate>
EventGeneratorDelegateAura::Create(ui::test::EventGenerator* owner,
                                   gfx::NativeWindow root_window,
                                   gfx::NativeWindow target_window) {
  // Tests should not create event generators for a "root window" that's not
  // actually the root window.
  if (root_window)
    DCHECK_EQ(root_window, root_window->GetRootWindow());

  return std::make_unique<DefaultEventGeneratorDelegate>(root_window);
}

EventGeneratorDelegateAura::EventGeneratorDelegateAura() = default;

EventGeneratorDelegateAura::~EventGeneratorDelegateAura() = default;

client::ScreenPositionClient*
EventGeneratorDelegateAura::GetScreenPositionClient(
    const Window* window) const {
  return client::GetScreenPositionClient(window->GetRootWindow());
}

void EventGeneratorDelegateAura::SetTargetWindow(
    gfx::NativeWindow target_window) {
  NOTIMPLEMENTED();
}

ui::EventSource* EventGeneratorDelegateAura::GetEventSource(
    ui::EventTarget* target) {
  return WindowFromTarget(target)->GetHost()->GetEventSource();
}

gfx::Point EventGeneratorDelegateAura::CenterOfTarget(
    const ui::EventTarget* target) const {
  return CenterOfWindow(WindowFromTarget(target));
}

gfx::Point EventGeneratorDelegateAura::CenterOfWindow(
    gfx::NativeWindow window) const {
  return CenterOfWindow(static_cast<const Window*>(window));
}

void EventGeneratorDelegateAura::ConvertPointFromTarget(
    const ui::EventTarget* event_target,
    gfx::Point* point) const {
  ConvertPointFromWindow(WindowFromTarget(event_target), point);
}

void EventGeneratorDelegateAura::ConvertPointToTarget(
    const ui::EventTarget* event_target,
    gfx::Point* point) const {
  DCHECK(point);
  const Window* target = WindowFromTarget(event_target);
  aura::client::ScreenPositionClient* client = GetScreenPositionClient(target);
  if (client)
    client->ConvertPointFromScreen(target, point);
  else
    aura::Window::ConvertPointToTarget(target->GetRootWindow(), target, point);
}

void EventGeneratorDelegateAura::ConvertPointFromWindow(
    gfx::NativeWindow window,
    gfx::Point* point) const {
  return ConvertPointFromWindow(static_cast<const Window*>(window), point);
}

void EventGeneratorDelegateAura::ConvertPointFromHost(
    const ui::EventTarget* hosted_target,
    gfx::Point* point) const {
  const Window* window = WindowFromTarget(hosted_target);
  window->GetHost()->ConvertPixelsToDIP(point);
}

gfx::Point EventGeneratorDelegateAura::CenterOfWindow(
    const Window* window) const {
  gfx::Point center = gfx::Rect(window->bounds().size()).CenterPoint();
  ConvertPointFromWindow(window, &center);
  return center;
}

void EventGeneratorDelegateAura::ConvertPointFromWindow(
    const Window* window,
    gfx::Point* point) const {
  DCHECK(point);
  aura::client::ScreenPositionClient* client = GetScreenPositionClient(window);
  if (client)
    client->ConvertPointToScreen(window, point);
  else
    aura::Window::ConvertPointToTarget(window, window->GetRootWindow(), point);
}

}  // namespace test
}  // namespace aura
