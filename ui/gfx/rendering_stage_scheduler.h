// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_RENDERING_STAGE_SCHEDULER_H_
#define UI_GFX_RENDERING_STAGE_SCHEDULER_H_

#include <memory>
#include <vector>

#include "base/threading/platform_thread.h"
#include "ui/gfx/gfx_export.h"

namespace gfx {

class GFX_EXPORT RenderingStageScheduler {
 public:
  // Creating instances of this class requires loading native libraries which
  // require synchronous file access. This method ensures the synchronous work
  // is finished.
  static void EnsureInitialized();

  static std::unique_ptr<RenderingStageScheduler> CreateAdpf(
      const char* pipeline_type,
      std::vector<base::PlatformThreadId> threads,
      base::TimeDelta desired_duration);

  virtual ~RenderingStageScheduler() = default;

  virtual void ReportCpuCompletionTime(base::TimeDelta actual_duration) = 0;
};

}  // namespace gfx

#endif  // UI_GFX_RENDERING_STAGE_SCHEDULER_H_
