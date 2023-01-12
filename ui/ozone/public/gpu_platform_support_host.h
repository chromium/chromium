// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PUBLIC_GPU_PLATFORM_SUPPORT_HOST_H_
#define UI_OZONE_PUBLIC_GPU_PLATFORM_SUPPORT_HOST_H_

#include <string>

#include "base/component_export.h"
#include "base/functional/callback_forward.h"
#include "mojo/public/cpp/system/message_pipe.h"

namespace ui {

// Platform-specific object to support a GPU process host.
//
// ChromeOS on bare hardware will do display configuration and cursor
// movement from the GPU process. This provides a conduit for the
// messages needed to make this work.
//
// Under X11, we don't need any GPU messages for display configuration.
// That's why there's no real functionality here: it's purely mechanism
// to support additional messages needed by specific platforms.
class COMPONENT_EXPORT(OZONE_BASE) GpuPlatformSupportHost {
 public:
  using GpuHostBindInterfaceCallback =
      base::RepeatingCallback<void(const std::string&,
                                   mojo::ScopedMessagePipeHandle)>;
  using GpuHostTerminateCallback =
      base::OnceCallback<void(const std::string& message)>;

  GpuPlatformSupportHost();
  virtual ~GpuPlatformSupportHost();

  // Called when the GPU process is destroyed.
  // This is called from browser UI thread.
  virtual void OnChannelDestroyed(int host_id) = 0;

  // Called when the GPU service is launched.
  // Called from the browser UI thread.
  virtual void OnGpuServiceLaunched(
      int host_id,
      GpuHostBindInterfaceCallback binder,
      GpuHostTerminateCallback terminate_callback) = 0;
};

// create a stub implementation.
COMPONENT_EXPORT(OZONE_BASE)
GpuPlatformSupportHost* CreateStubGpuPlatformSupportHost();

}  // namespace ui

#endif  // UI_OZONE_PUBLIC_GPU_PLATFORM_SUPPORT_HOST_H_
