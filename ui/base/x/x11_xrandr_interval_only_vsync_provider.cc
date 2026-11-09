// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/x/x11_xrandr_interval_only_vsync_provider.h"

#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "base/task/single_thread_task_runner.h"
#include "ui/base/x/x11_display_util.h"
#include "ui/display/types/display_config.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/event.h"
#include "ui/gfx/x/future.h"
#include "ui/gfx/x/geometry_cache.h"
#include "ui/gfx/x/randr.h"

namespace ui {
namespace {

constexpr base::TimeDelta kDefaultInterval = base::Seconds(1) / 60;

bool IsXrandrAvailable() {
  constexpr std::pair<uint32_t, uint32_t> kMinXrandrVersion{1, 3};
  return x11::Connection::Get()->randr_version() >= kMinXrandrVersion;
}

// Caches the XRandR display configuration until it changes.
class XRandrHelper : public x11::EventObserver {
 public:
  XRandrHelper() {
    CHECK(IsXrandrAvailable(), base::NotFatalUntil::M163);
    auto* connection = x11::Connection::Get();
    auto root_window = connection->default_screen().root;
    auto& randr = connection->randr();
    // CrtcChange is needed to observe mode (and thus refresh rate) changes.
    auto randr_event_mask = x11::RandR::NotifyMask::OutputChange |
                            x11::RandR::NotifyMask::CrtcChange;
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
      displays_.reset();
    }
  }

  static base::TimeDelta GetInterval(
      const std::optional<gfx::Rect>& bounds_px) {
    if (IsXrandrAvailable()) {
      static base::NoDestructor<XRandrHelper> helper;
      return helper->GetIntervalInternal(bounds_px);
    }
    return kDefaultInterval;
  }

 private:
  base::TimeDelta GetIntervalInternal(
      const std::optional<gfx::Rect>& bounds_px) {
    if (!displays_) {
      displays_ = BuildDisplaysFromXRandRInfo(display::DisplayConfig(),
                                              &primary_display_index_);
    }
    return GetDisplayRefreshInterval(*displays_, primary_display_index_,
                                     bounds_px);
  }

  std::optional<std::vector<display::Display>> displays_;
  size_t primary_display_index_ = 0;
};

}  // namespace

XrandrIntervalOnlyVSyncProvider::XrandrIntervalOnlyVSyncProvider(
    x11::Window window) {
  if (window != x11::Window::None) {
    geometry_cache_ = std::make_unique<x11::GeometryCache>(
        x11::Connection::Get(), window,
        base::BindRepeating(
            &XrandrIntervalOnlyVSyncProvider::OnWindowBoundsChanged,
            weak_factory_.GetWeakPtr()));
  }
}

XrandrIntervalOnlyVSyncProvider::~XrandrIntervalOnlyVSyncProvider() = default;

void XrandrIntervalOnlyVSyncProvider::GetVSyncParameters(
    UpdateVSyncCallback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), base::TimeTicks(),
                                XRandrHelper::GetInterval(window_bounds_)));
}

bool XrandrIntervalOnlyVSyncProvider::GetVSyncParametersIfAvailable(
    base::TimeTicks* timebase,
    base::TimeDelta* interval) {
  *interval = XRandrHelper::GetInterval(window_bounds_);
  return true;
}

bool XrandrIntervalOnlyVSyncProvider::SupportGetVSyncParametersIfAvailable()
    const {
  return true;
}

bool XrandrIntervalOnlyVSyncProvider::IsHWClock() const {
  return false;
}

void XrandrIntervalOnlyVSyncProvider::OnWindowBoundsChanged(
    const std::optional<gfx::Rect>& old_bounds,
    const gfx::Rect& new_bounds) {
  window_bounds_ = new_bounds;
}

}  // namespace ui
