// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/rendering_stage_scheduler.h"

#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"

#if defined(OS_ANDROID)
#include "base/android/jni_array.h"
#include "base/android/scoped_java_ref.h"
#include "ui/gfx/gfx_jni_headers/AdpfRenderingStageScheduler_jni.h"
#endif  // OS_ANDROID

namespace gfx {
namespace {
#if defined(OS_ANDROID)

class RenderingStageSchedulerAdpf : public RenderingStageScheduler {
 public:
  RenderingStageSchedulerAdpf(const char* pipeline_type,
                              std::vector<base::PlatformThreadId> threads,
                              base::TimeDelta desired_duration)
      : pipeline_type_(pipeline_type), desired_duration_(desired_duration) {
    static_assert(sizeof(base::PlatformThreadId) == sizeof(jint),
                  "thread id types incompatible");
    JNIEnv* env = base::android::AttachCurrentThread();
    j_object_ = Java_AdpfRenderingStageScheduler_create(
        env, base::android::ToJavaIntArray(env, threads),
        desired_duration_.InNanoseconds());
  }

  ~RenderingStageSchedulerAdpf() override {
    if (!j_object_)
      return;
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_AdpfRenderingStageScheduler_destroy(env, j_object_);
  }

  void ReportCpuCompletionTime(base::TimeDelta actual_duration) override {
    TRACE_EVENT_INSTANT2(
        "benchmark", "RenderingStageSchedulerAdpf::ReportCpuCompletionTime",
        TRACE_EVENT_SCOPE_THREAD, "pipeline_type", pipeline_type_,
        "utilization_percentage",
        static_cast<int>(actual_duration * 100 / desired_duration_));
    if (!j_object_)
      return;
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_AdpfRenderingStageScheduler_reportCpuCompletionTime(
        env, j_object_, actual_duration.InNanoseconds());
  }

 private:
  const char* pipeline_type_;
  const base::TimeDelta desired_duration_;
  base::android::ScopedJavaGlobalRef<jobject> j_object_;
};

#endif  // OS_ANDROID

}  // namespace

std::unique_ptr<RenderingStageScheduler> RenderingStageScheduler::CreateAdpf(
    const char* pipeline_type,
    std::vector<base::PlatformThreadId> threads,
    base::TimeDelta desired_duration) {
#if defined(OS_ANDROID)
  return std::make_unique<RenderingStageSchedulerAdpf>(
      pipeline_type, std::move(threads), desired_duration);
#endif
  return nullptr;
}

}  // namespace gfx
