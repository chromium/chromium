// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PUBLIC_OZONE_GPU_TEST_HELPER_H_
#define UI_OZONE_PUBLIC_OZONE_GPU_TEST_HELPER_H_

#include <memory>

#include "base/component_export.h"

namespace ui {

class FakeGpuConnection;

// Helper class for applications that do not have a dedicated GPU channel.
//
// This sets up mojo pipe between the "gpu" and "ui" threads. It is not needed
// if in Mojo single-thread mode.
class COMPONENT_EXPORT(OZONE) OzoneGpuTestHelper {
 public:
  OzoneGpuTestHelper();

  OzoneGpuTestHelper(const OzoneGpuTestHelper&) = delete;
  OzoneGpuTestHelper& operator=(const OzoneGpuTestHelper&) = delete;

  virtual ~OzoneGpuTestHelper();

  // Binds mojo endpoints on "gpu" and "ui".
  bool Initialize();

 private:
  std::unique_ptr<FakeGpuConnection> fake_gpu_connection_;
};

}  // namespace ui

#endif  // UI_OZONE_PUBLIC_OZONE_GPU_TEST_HELPER_H_
