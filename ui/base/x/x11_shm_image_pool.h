// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_X_X11_SHM_IMAGE_POOL_H_
#define UI_BASE_X_X11_SHM_IMAGE_POOL_H_

#include "base/component_export.h"
#include "base/macros.h"
#include "ui/base/x/x11_shm_image_pool_base.h"
#include "ui/events/platform/platform_event_dispatcher.h"

namespace ui {

// TODO(crbug.com/965991): merge this with the base class when separation for
// X11 and ozone/x11 is not needed.
class COMPONENT_EXPORT(UI_BASE_X) X11ShmImagePool
    : public XShmImagePoolBase,
      public PlatformEventDispatcher {
 public:
  X11ShmImagePool(base::TaskRunner* host_task_runner,
                  base::TaskRunner* event_task_runner,
                  XDisplay* display,
                  XID drawable,
                  Visual* visual,
                  int depth,
                  std::size_t max_frames_pending);

 private:
  ~X11ShmImagePool() override;

  // XShmImagePoolBase:
  void AddEventDispatcher() override;
  void RemoveEventDispatcher() override;

  // PlatformEventDispatcher:
  bool CanDispatchEvent(const PlatformEvent& event) override;
  uint32_t DispatchEvent(const PlatformEvent& event) override;

  DISALLOW_COPY_AND_ASSIGN(X11ShmImagePool);
};

}  // namespace ui

#endif  // UI_BASE_X_X11_SHM_IMAGE_POOL_H_
