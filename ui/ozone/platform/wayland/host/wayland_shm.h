// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_SHM_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_SHM_H_

#include <memory>

#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"
#include "ui/ozone/platform/wayland/common/wayland_util.h"

namespace ui {

class WaylandConnection;

// Wrapper around |wl_shm| Wayland factory, which creates
// |wl_buffer|s backed by a fd to a shared memory.
class WaylandShm {
 public:
  WaylandShm(wl_shm* shm, WaylandConnection* connection);
  ~WaylandShm();

  // Creates a wl_buffer based on shared memory handle for the specified
  // |widget|.
  wl::Object<struct wl_buffer> CreateBuffer(base::ScopedFD fd,
                                            size_t length,
                                            const gfx::Size& size);

 private:
  wl::Object<wl_shm> const shm_;

  // Non-owned pointer to the main connection.
  WaylandConnection* const connection_;

  DISALLOW_COPY_AND_ASSIGN(WaylandShm);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_SHM_H_
