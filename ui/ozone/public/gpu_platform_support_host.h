// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PUBLIC_GPU_PLATFORM_SUPPORT_HOST_H_
#define UI_OZONE_PUBLIC_GPU_PLATFORM_SUPPORT_HOST_H_

#include <string>

#include "base/component_export.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_sender.h"
#include "mojo/public/cpp/bindings/binding.h"

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

  // Called when the GPU process is spun up.
  // This is called from browser IO thread.
  virtual void OnGpuProcessLaunched(
      int host_id,
      scoped_refptr<base::SingleThreadTaskRunner> ui_runner,
      scoped_refptr<base::SingleThreadTaskRunner> send_runner,
      base::RepeatingCallback<void(IPC::Message*)> sender) = 0;

  // Called when the GPU process is destroyed.
  // This is called from browser UI thread.
  virtual void OnChannelDestroyed(int host_id) = 0;

  // Called to handle an IPC message. Note that this can be called from any
  // thread.
  virtual void OnMessageReceived(const IPC::Message& message) = 0;

  // Called when the GPU service is launched.
  // Called from the browser IO thread.
  virtual void OnGpuServiceLaunched(
      int host_id,
      scoped_refptr<base::SingleThreadTaskRunner> host_runner,
      scoped_refptr<base::SingleThreadTaskRunner> io_runner,
      GpuHostBindInterfaceCallback binder,
      GpuHostTerminateCallback terminate_callback) = 0;
};

// create a stub implementation.
COMPONENT_EXPORT(OZONE_BASE)
GpuPlatformSupportHost* CreateStubGpuPlatformSupportHost();

}  // namespace ui

#endif  // UI_OZONE_PUBLIC_GPU_PLATFORM_SUPPORT_HOST_H_
