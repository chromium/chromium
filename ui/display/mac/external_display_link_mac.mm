// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/mac/external_display_link_mac.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/bind_post_task.h"
#include "base/trace_event/trace_event.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"
#include "ui/display/mac/screen_utils_mac.h"

namespace ui {

namespace {
struct DisplayLinkGlobals {
  DisplayLinkGlobals() = default;
  base::Lock lock;

  // Indicates whether the display creation has been logged within the
  // 'Viz.ExternalBeginFrameSourceMac.DisplayLink.Create2' histogram.
  absl::flat_hash_set<CGDirectDisplayID> recorded_displays GUARDED_BY(lock);

  static DisplayLinkGlobals& Get() {
    static base::NoDestructor<DisplayLinkGlobals> instance;
    return *instance;
  }
};
}  // namespace

// static
void ExternalDisplayLinkMac::TryRecordDisplayLinkCreation(
    CGDirectDisplayID display_id,
    bool success) {
  auto& globals = DisplayLinkGlobals::Get();
  base::AutoLock lock(globals.lock);

  auto [it, inserted] = globals.recorded_displays.insert(display_id);
  if (inserted) {
    UMA_HISTOGRAM_BOOLEAN("Viz.DisplayLink.Create.GPU.ExternalDisplayLink",
                          success);
  }
}
// static
scoped_refptr<DisplayLinkMac> ExternalDisplayLinkMac::GetForDisplay(
    CGDirectDisplayID display_id) {
  TRACE_EVENT("gpu", "ExternalDisplayLinkMac::GetForDisplay");

  if (!VSyncProviderMac::GetInstance()->IsDisplayLinkInBrowserValid(
          display_id)) {
    TryRecordDisplayLinkCreation(display_id, /*success=*/false);
    return nullptr;
  }

  TryRecordDisplayLinkCreation(display_id, /*success=*/true);
  return (new ExternalDisplayLinkMac(display_id));
}

ExternalDisplayLinkMac::ExternalDisplayLinkMac(CGDirectDisplayID display_id)
    : display_id_(display_id),
      vsync_provider_(VSyncProviderMac::GetInstance()),
      post_callback_to_ctor_thread_(
          !vsync_provider_->BelongsToCurrentThread()) {}

ExternalDisplayLinkMac::~ExternalDisplayLinkMac() {
  if (callback_for_providers_thread_) {
    vsync_provider_->UnregisterCallback(
        std::move(callback_for_providers_thread_), display_id_);
  }
}

std::unique_ptr<VSyncCallbackMac> ExternalDisplayLinkMac::RegisterCallback(
    VSyncCallbackMac::Callback callback) {
  CHECK(!callback_for_providers_thread_);
  TRACE_EVENT("gpu", "ExternalDisplayLinkMac::RegisterCallback");

  std::unique_ptr<VSyncCallbackMac> new_callback =
      base::WrapUnique(new VSyncCallbackMac(
          base::BindOnce(&ExternalDisplayLinkMac::UnregisterCallback,
                         weak_factory_.GetWeakPtr()),
          std::move(callback), /*post_callback_to_ctor_thread*/ false));

  // Set up a callback for VSyncProviderMac which runs on the Viz thread.
  auto lambda = [](base::WeakPtr<VSyncCallbackMac> weak_this,
                   VSyncParamsMac params) {
    if (weak_this) {
      weak_this->callback_for_displaylink_thread_.Run(params);
    }
  };

  VSyncCallbackMac::Callback callback_for_current_thread =
      base::BindRepeating(lambda, new_callback->GetWeakPtr());

  VSyncCallbackMac::Callback callback_for_providers_thread;
  if (post_callback_to_ctor_thread_) {
    callback_for_providers_thread_ = base::BindPostTaskToCurrentDefault(
        std::move(callback_for_current_thread));
  } else {
    callback_for_providers_thread_ = std::move(callback_for_current_thread);
  }

  vsync_provider_->RegisterCallback(callback_for_providers_thread_,
                                    display_id_);

  return new_callback;
}

void ExternalDisplayLinkMac::UnregisterCallback(VSyncCallbackMac* callback) {
  CHECK(callback_for_providers_thread_);
  TRACE_EVENT("gpu", "ExternalDisplayLinkMac::UnregisterCallback");
  vsync_provider_->UnregisterCallback(std::move(callback_for_providers_thread_),
                                      display_id_);
}

base::TimeDelta ExternalDisplayLinkMac::GetRefreshInterval() const {
  return display::GetNSScreenRefreshInterval(display_id_);
}

void ExternalDisplayLinkMac::GetRefreshIntervalRange(
    base::TimeDelta& min_interval,
    base::TimeDelta& max_interval,
    base::TimeDelta& granularity) const {
  display::GetNSScreenRefreshIntervalRange(display_id_, min_interval,
                                           max_interval, granularity);
}

base::TimeTicks ExternalDisplayLinkMac::GetCurrentTime() const {
  return base::TimeTicks::Now();
}

}  // namespace ui
