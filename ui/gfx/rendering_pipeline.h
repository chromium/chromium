// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_RENDERING_PIPELINE_H_
#define UI_GFX_RENDERING_PIPELINE_H_

#include "base/containers/flat_map.h"
#include "base/memory/singleton.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/platform_thread.h"
#include "ui/gfx/gfx_export.h"

namespace base {
namespace sequence_manager {
class TaskTimeObserver;
}
}  // namespace base

namespace gfx {

// Tracks the desired and actual execution time of rendering threads to
// optimally schedule them on the CPU. Instances of this class should be shared
// between all compositors of the same rendering stage.
//
// This class can be created on any thread but becomes bound to the thread it's
// subsequently used on. The class may be destroyed on any thread but the caller
// is responsible for ensuring all other threads in the rendering stage, other
// than the thread the object is destroyed on, are torn down before destroying
// an instance of this class.
class GFX_EXPORT RenderingPipeline {
 public:
  // Notifies when this pipeline is active. Multiple pipelines of the same type
  // can be concurrently active at a time. The pipeline is assumed active for
  // the lifetime of this object.
  class GFX_EXPORT ScopedPipelineActive {
   public:
    explicit ScopedPipelineActive(RenderingPipeline* pipeline);
    ~ScopedPipelineActive();

   private:
    RenderingPipeline* const pipeline_;
  };

  static std::unique_ptr<RenderingPipeline> CreateRendererMain();
  static std::unique_ptr<RenderingPipeline> CreateRendererCompositor();
  static std::unique_ptr<RenderingPipeline> CreateGpu();

  virtual ~RenderingPipeline() = default;

  // Add to this pipeline a thread backed by base sequence manager, where
  // |base::CurrentThread| works. Most threads in chromium should fall into
  // this category.
  // This method is thread safe and can be called on any thread.
  virtual void AddSequenceManagerThread(
      base::PlatformThreadId thread_id,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner) = 0;

  // Add a simple thread to this pipeline. The caller is responsible for
  // updating the returned observer for tasks executed on the thread.
  // The returned observer is owned by this pipeline object.
  // This method is thread safe and can be called on any thread.
  virtual base::sequence_manager::TaskTimeObserver* AddSimpleThread(
      base::PlatformThreadId thread_id) = 0;

  // Notifies when this pipeline stage has finished rendering to compute the
  // execution time per frame for the associated threads.
  virtual void NotifyFrameFinished() = 0;

  // Sets the desired duration for this pipeline.
  virtual void SetTargetDuration(base::TimeDelta target_duration) = 0;

  // Sets the latency from composition for a display buffer finishing on the
  // Gpu thread to when execution finished on the Gpu.
  virtual void SetGpuLatency(base::TimeDelta delta) = 0;

 protected:
  virtual void UpdateActiveCount(bool active) = 0;
};

}  // namespace gfx

#endif  // UI_GFX_RENDERING_PIPELINE_H_
