// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/x/x11_shm_image_pool_ozone.h"

#include "base/bind.h"
#include "base/location.h"
#include "build/build_config.h"

namespace ui {

X11ShmImagePoolOzone::X11ShmImagePoolOzone(base::TaskRunner* host_task_runner,
                                           base::TaskRunner* event_task_runner,
                                           XDisplay* display,
                                           XID drawable,
                                           Visual* visual,
                                           int depth,
                                           std::size_t frames_pending)
    : XShmImagePoolBase(host_task_runner,
                        event_task_runner,
                        display,
                        drawable,
                        visual,
                        depth,
                        frames_pending) {}

X11ShmImagePoolOzone::~X11ShmImagePoolOzone() = default;

void X11ShmImagePoolOzone::AddEventDispatcher() {
  ui::X11EventSource::GetInstance()->AddXEventDispatcher(this);

#ifndef NDEBUG
  dispatcher_registered_ = true;
#endif
}

void X11ShmImagePoolOzone::RemoveEventDispatcher() {
  ui::X11EventSource::GetInstance()->RemoveXEventDispatcher(this);

#ifndef NDEBUG
  dispatcher_registered_ = false;
#endif
}

bool X11ShmImagePoolOzone::DispatchXEvent(XEvent* xev) {
  if (!CanDispatchXEvent(xev))
    return false;

  XShmCompletionEvent* shm_event = reinterpret_cast<XShmCompletionEvent*>(xev);
  host_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&X11ShmImagePoolOzone::DispatchShmCompletionEvent, this,
                     *shm_event));
  return true;
}

}  // namespace ui
