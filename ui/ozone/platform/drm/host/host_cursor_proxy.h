// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_HOST_HOST_CURSOR_PROXY_H_
#define UI_OZONE_PLATFORM_DRM_HOST_HOST_CURSOR_PROXY_H_

#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/platform/drm/host/drm_cursor.h"
#include "ui/ozone/public/mojom/device_cursor.mojom.h"

namespace ui {

// Ozone requires a IPC from the browser (or mus-ws) process to the gpu (or
// mus-gpu) process to control the mouse pointer. This class provides mouse
// pointer control via Mojo-style IPC. This code runs only in the mus-ws (i.e.
// it's the client) and sends mouse pointer control messages to a less
// priviledged process.
class HostCursorProxy : public DrmCursorProxy {
 public:
  HostCursorProxy(
      mojo::PendingAssociatedRemote<ui::ozone::mojom::DeviceCursor> main_cursor,
      mojo::PendingAssociatedRemote<ui::ozone::mojom::DeviceCursor>
          evdev_cursor);
  ~HostCursorProxy() override;

 private:
  // DrmCursorProxy.
  void CursorSet(gfx::AcceleratedWidget window,
                 const std::vector<SkBitmap>& bitmaps,
                 const gfx::Point& point,
                 int frame_delay_ms) override;
  void Move(gfx::AcceleratedWidget window, const gfx::Point& point) override;
  void InitializeOnEvdevIfNecessary() override;

  // Mojo implementation of the DrmCursorProxy.
  mojo::AssociatedRemote<ui::ozone::mojom::DeviceCursor> main_cursor_;
  mojo::AssociatedRemote<ui::ozone::mojom::DeviceCursor> evdev_cursor_;

  base::PlatformThreadRef ui_thread_ref_;
  bool evdev_bound_ = false;

  DISALLOW_COPY_AND_ASSIGN(HostCursorProxy);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_HOST_HOST_CURSOR_PROXY_H_
