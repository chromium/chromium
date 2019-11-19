// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PUBLIC_OZONE_GPU_THREAD_HELPER_H_
#define UI_OZONE_PUBLIC_OZONE_GPU_THREAD_HELPER_H_

#include <memory>

#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"

namespace base {
class SingleThreadTaskRunner;
class Thread;
}

namespace ui {

class FakeGpuProcess;
class FakeGpuProcessHost;

// Helper class for applications that do not have a dedicated GPU channel.
//
// This sets up message forwarding between the "gpu" and "ui" threads.
class COMPONENT_EXPORT(OZONE) OzoneGpuTestHelper {
 public:
  OzoneGpuTestHelper();
  virtual ~OzoneGpuTestHelper();

  // Start processing gpu messages. The "gpu" process will be using the
  // |ui_task_runner| to post messages intended for the "ui" thread.
  bool Initialize(
      const scoped_refptr<base::SingleThreadTaskRunner>& ui_task_runner);

 private:
  std::unique_ptr<FakeGpuProcess> fake_gpu_process_;
  std::unique_ptr<FakeGpuProcessHost> fake_gpu_process_host_;
  std::unique_ptr<base::Thread> io_helper_thread_;

  DISALLOW_COPY_AND_ASSIGN(OzoneGpuTestHelper);
};

}  // namespace ui

#endif  // UI_OZONE_PUBLIC_OZONE_GPU_THREAD_HELPER_H_
