// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/mac/external_display_link_mac.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/task/bind_post_task.h"
#include "ui/display/mac/screen_utils_mac.h"

namespace ui {

// static
scoped_refptr<DisplayLinkMac> ExternalDisplayLinkMac::GetForDisplay(
    int64_t display_id) {
  if (!IsDisplayLinkSupported(display_id)) {
    return nullptr;
  }

  return (new ExternalDisplayLinkMac(display_id));
}

// static
bool ExternalDisplayLinkMac::IsDisplayLinkSupported(int64_t display_id) {
  return VSyncProviderMac::GetInstance()->IsDisplayLinkSupported(display_id);
}

ExternalDisplayLinkMac::ExternalDisplayLinkMac(int64_t display_id)
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
