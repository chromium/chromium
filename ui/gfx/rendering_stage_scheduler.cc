// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/rendering_stage_scheduler.h"

#include "base/logging.h"
#include "base/native_library.h"
#include "base/no_destructor.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"

#if defined(OS_ANDROID)

#include <dlfcn.h>
#include <sys/types.h>

extern "C" {

typedef struct APowerManager APowerManager;

using pAPower_acquireManager = APowerManager* (*)();
using pAPower_createThreadGroup = int64_t (*)(APowerManager* manager,
                                              pid_t* threadIds,
                                              size_t numThreadIds,
                                              uint64_t desiredDurationMicros);
using pAPower_destroyThreadGroup =
    void (*)(APowerManager* manager, int64_t /* ThreadGroupId */ threadGroup);
using pAPower_updateThreadGroupDesiredDuration =
    void (*)(APowerManager* manager,
             int64_t /* ThreadGroupId */ threadGroup,
             uint64_t desiredDurationMicros);
using pAPower_reportThreadGroupDuration =
    void (*)(APowerManager* manager,
             int64_t /* ThreadGroupId */ threadGroup,
             uint64_t actualDurationMicros);
}

#endif  // OS_ANDROID

namespace gfx {
namespace {

#if defined(OS_ANDROID)

#define LOAD_FUNCTION(lib, func)                                \
  do {                                                          \
    func##Fn = reinterpret_cast<p##func>(                       \
        base::GetFunctionPointerFromNativeLibrary(lib, #func)); \
    if (!func##Fn) {                                            \
      supported = false;                                        \
      LOG(ERROR) << "Unable to load function " << #func;        \
    }                                                           \
  } while (0)

struct AdpfMethods {
  static const AdpfMethods& Get() {
    static const base::NoDestructor<AdpfMethods> instance;
    return *instance;
  }

  AdpfMethods() {
    base::NativeLibraryLoadError error;
    base::NativeLibrary main_dl_handle =
        base::LoadNativeLibrary(base::FilePath("libandroid.so"), &error);
    if (!main_dl_handle) {
      LOG(ERROR) << "Couldnt load libandroid.so: " << error.ToString();
      supported = false;
      return;
    }

    LOAD_FUNCTION(main_dl_handle, APower_acquireManager);
    LOAD_FUNCTION(main_dl_handle, APower_createThreadGroup);
    LOAD_FUNCTION(main_dl_handle, APower_destroyThreadGroup);
    LOAD_FUNCTION(main_dl_handle, APower_updateThreadGroupDesiredDuration);
    LOAD_FUNCTION(main_dl_handle, APower_reportThreadGroupDuration);
  }

  ~AdpfMethods() = default;

  bool supported = true;
  pAPower_acquireManager APower_acquireManagerFn;
  pAPower_createThreadGroup APower_createThreadGroupFn;
  pAPower_destroyThreadGroup APower_destroyThreadGroupFn;
  pAPower_updateThreadGroupDesiredDuration
      APower_updateThreadGroupDesiredDurationFn;
  pAPower_reportThreadGroupDuration APower_reportThreadGroupDurationFn;
};

APowerManager* GetPowerManager() {
  static APowerManager* power_manager =
      AdpfMethods::Get().supported
          ? AdpfMethods::Get().APower_acquireManagerFn()
          : nullptr;
  return power_manager;
}

class RenderingStageSchedulerAdpf : public RenderingStageScheduler {
 public:
  RenderingStageSchedulerAdpf(const char* pipeline_type,
                              std::vector<base::PlatformThreadId> threads,
                              base::TimeDelta desired_duration)
      : pipeline_type_(pipeline_type), desired_duration_(desired_duration) {
    static_assert(sizeof(base::PlatformThreadId) == sizeof(pid_t),
                  "thread id types incompatible");

    if (!GetPowerManager())
      return;

    id_ = AdpfMethods::Get().APower_createThreadGroupFn(
        GetPowerManager(), threads.data(), threads.size(),
        desired_duration.InMicroseconds());
  }

  ~RenderingStageSchedulerAdpf() override {
    if (!GetPowerManager())
      return;

    AdpfMethods::Get().APower_destroyThreadGroupFn(GetPowerManager(), id_);
  }

  void ReportCpuCompletionTime(base::TimeDelta actual_duration) override {
    TRACE_EVENT_INSTANT2(
        "benchmark", "RenderingStageSchedulerAdpf::ReportCpuCompletionTime",
        TRACE_EVENT_SCOPE_THREAD, "pipeline_type", pipeline_type_,
        "utilization_percentage",
        static_cast<int>(actual_duration * 100 / desired_duration_));

    if (!GetPowerManager())
      return;

    AdpfMethods::Get().APower_reportThreadGroupDurationFn(
        GetPowerManager(), id_, actual_duration.InMicroseconds());
  }

 private:
  int64_t id_;
  const char* pipeline_type_;
  const base::TimeDelta desired_duration_;
};

#endif  // OS_ANDROID

}  // namespace

void RenderingStageScheduler::EnsureInitialized() {
#if defined(OS_ANDROID)
  AdpfMethods::Get();
#endif
}

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
