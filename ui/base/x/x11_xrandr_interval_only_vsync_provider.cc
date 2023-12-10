// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/x/x11_xrandr_interval_only_vsync_provider.h"

#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "base/task/single_thread_task_runner.h"
#include "ui/base/x/x11_display_util.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/event.h"
#include "ui/gfx/x/future.h"
#include "ui/gfx/x/randr.h"

namespace ui {
namespace {

bool IsXrandrAvailable() {
  constexpr std::pair<uint32_t, uint32_t> kMinXrandrVersion{1, 3};
  return x11::Connection::Get()->randr_version() >= kMinXrandrVersion;
}

class XRandrHelper : public x11::EventObserver {
 public:
  XRandrHelper() {
    DCHECK(IsXrandrAvailable());
    auto* connection = x11::Connection::Get();
    auto root_window = connection->default_screen().root;
    auto& randr = connection->randr();
    auto randr_event_mask = x11::RandR::NotifyMask::OutputChange;
    randr.SelectInput({root_window, randr_event_mask});
    connection->AddEventObserver(this);
  }

  ~XRandrHelper() override {
    auto* connection = x11::Connection::Get();
    connection->RemoveEventObserver(this);
  }

  // x11::EventObserver:
  void OnEvent(const x11::Event& xevent) override {
    if (xevent.As<x11::RandR::NotifyEvent>()) {
      interval_ = {};
    }
  }

  static base::TimeDelta GetInterval() {
    if (IsXrandrAvailable()) {
      static base::NoDestructor<XRandrHelper> helper;
      return helper->GetIntervalInternal();
    }
    constexpr auto kDefaultInterval = base::Seconds(1) / 60;
    return kDefaultInterval;
  }

 private:
  base::TimeDelta GetIntervalInternal() {
    if (interval_.is_zero()) {
      interval_ = GetPrimaryDisplayRefreshIntervalFromXrandr();
    }
    return interval_;
  }

  base::TimeDelta interval_;
};

}  // namespace

XrandrIntervalOnlyVSyncProvider::XrandrIntervalOnlyVSyncProvider() = default;
XrandrIntervalOnlyVSyncProvider::~XrandrIntervalOnlyVSyncProvider() = default;

void XrandrIntervalOnlyVSyncProvider::GetVSyncParameters(
    UpdateVSyncCallback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), base::TimeTicks(),
                                XRandrHelper::GetInterval()));
}

bool XrandrIntervalOnlyVSyncProvider::GetVSyncParametersIfAvailable(
    base::TimeTicks* timebase,
    base::TimeDelta* interval) {
  *interval = XRandrHelper::GetInterval();
  return true;
}

bool XrandrIntervalOnlyVSyncProvider::SupportGetVSyncParametersIfAvailable()
    const {
  return true;
}

bool XrandrIntervalOnlyVSyncProvider::IsHWClock() const {
  return false;
}

}  // namespace ui
