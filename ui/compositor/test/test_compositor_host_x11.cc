// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/test/test_compositor_host_x11.h"

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "ui/events/x/x11_window_event_manager.h"
#include "ui/gfx/x/xproto.h"

namespace ui {

TestCompositorHostX11::TestCompositorHostX11(
    const gfx::Rect& bounds,
    ui::ContextFactory* context_factory)
    : bounds_(bounds),
      context_factory_(context_factory),
      compositor_(context_factory_->AllocateFrameSinkId(),
                  context_factory_,
                  base::ThreadTaskRunnerHandle::Get(),
                  false /* enable_pixel_canvas */) {}

TestCompositorHostX11::~TestCompositorHostX11() = default;

void TestCompositorHostX11::Show() {
  auto* connection = x11::Connection::Get();
  window_ = connection->GenerateId<x11::Window>();
  connection->CreateWindow({
      .wid = window_,
      .parent = connection->default_root(),
      .x = bounds_.x(),
      .y = bounds_.y(),
      .width = bounds_.width(),
      .height = bounds_.height(),
      .c_class = x11::WindowClass::InputOutput,
      .override_redirect = x11::Bool32(true),
  });
  window_events_ =
      std::make_unique<XScopedEventSelector>(window_, x11::EventMask::Exposure);
  connection->MapWindow({window_});
  // Since this window is override-redirect, syncing is sufficient
  // to ensure the map is complete.
  connection->Sync();
  allocator_.GenerateId();
  compositor_.SetAcceleratedWidget(
      static_cast<gfx::AcceleratedWidget>(window_));
  compositor_.SetScaleAndSize(1.0f, bounds_.size(),
                              allocator_.GetCurrentLocalSurfaceId());
  compositor_.SetVisible(true);
}

ui::Compositor* TestCompositorHostX11::GetCompositor() {
  return &compositor_;
}

}  // namespace ui
