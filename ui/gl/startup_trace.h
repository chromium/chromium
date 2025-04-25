// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_STARTUP_TRACE_H_
#define UI_GL_STARTUP_TRACE_H_

#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "ui/gl/gl_export.h"

namespace base {
class SequencedTaskRunner;
}

namespace gl {

// This class provides a method to trace the initialization of GPU process. When
// tracing instance is not initialized it will accumulate stages with start and
// end time stamp so they can emit traces once tracing inits. Calling this after
// `StartupDone()` will not add a stage. The caveat is that it cannot know
// before-hand whether the tracing category is on or not so the stage is
// recorded nonetheless.
//
// Example:
//   GPU_STARTUP_TRACE_EVENT(__func__);
//
// Note: Currently it's only safe to invoke `GPU_STARTUP_TRACE_EVENT` from the
// gpu main thread between `Startup()` and `StartupDone()`.
class GL_EXPORT StartupTrace {
 public:
  struct Stage {
    const char* name = nullptr;
    const base::TimeTicks start;
    base::TimeTicks end;
  };

  class GL_EXPORT ScopedStage {
   public:
    explicit ScopedStage(size_t size);
    ~ScopedStage();

   private:
    size_t size = 0;
  };

  StartupTrace();
  StartupTrace(const StartupTrace&) = delete;
  StartupTrace(StartupTrace&&) = delete;
  StartupTrace& operator=(const StartupTrace&) = delete;
  StartupTrace& operator=(StartupTrace&&) = delete;
  ~StartupTrace();

  static StartupTrace* GetInstance();
  ALWAYS_INLINE static bool IsEnabled() {
    return startup_in_progress_.load(std::memory_order_acquire);
  }

  void BindToCurrentThread();
  ScopedStage AddStage(const char* name);

  // Called by the main thread before/after the tracing.
  static void Startup();
  static void StarupDone();

 private:
  void RecordAndClearStages();

  static std::atomic<bool> startup_in_progress_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  std::vector<Stage> stages_;
};

}  // namespace gl

// Generate a unique variable name with a given prefix.
#define GPU_STARTUP_TRACE_INTERNAL_CONCAT2(a, b) a##b
#define GPU_STARTUP_TRACE_INTERNAL_CONCAT(a, b) \
  GPU_STARTUP_TRACE_INTERNAL_CONCAT2(a, b)
#define GPU_STARTUP_TRACE_UID(prefix) \
  GPU_STARTUP_TRACE_INTERNAL_CONCAT(prefix, __LINE__)

#define GPU_STARTUP_TRACE_EVENT(name)                                       \
  gl::StartupTrace::ScopedStage GPU_STARTUP_TRACE_UID(scoped_gpu_trace){0}; \
  if (gl::StartupTrace::IsEnabled()) {                                      \
    GPU_STARTUP_TRACE_UID(scoped_gpu_trace) =                               \
        gl::StartupTrace::GetInstance()->AddStage(name);                    \
  } else {                                                                  \
    TRACE_EVENT0("gpu,startup", name);                                      \
  }

#endif  // UI_GL_STARTUP_TRACE_H_
