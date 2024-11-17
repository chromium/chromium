// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gpu_switching_manager.h"

#include "base/observer_list.h"

namespace ui {

// static
GpuSwitchingManager* GpuSwitchingManager::GetInstance() {
  return base::Singleton<GpuSwitchingManager>::get();
}

GpuSwitchingManager::GpuSwitchingManager() = default;

GpuSwitchingManager::~GpuSwitchingManager() = default;

void GpuSwitchingManager::AddObserver(GpuSwitchingObserver* observer) {
  observer_list_.AddObserver(observer);
}

void GpuSwitchingManager::RemoveObserver(GpuSwitchingObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

void GpuSwitchingManager::NotifyGpuSwitched(
    gl::GpuPreference active_gpu_heuristic) {
  observer_list_.Notify(&GpuSwitchingObserver::OnGpuSwitched,
                        active_gpu_heuristic);
}

void GpuSwitchingManager::NotifyDisplayAdded() {
  observer_list_.Notify(&GpuSwitchingObserver::OnDisplayAdded);
}

void GpuSwitchingManager::NotifyDisplayRemoved() {
  observer_list_.Notify(&GpuSwitchingObserver::OnDisplayRemoved);
}

void GpuSwitchingManager::NotifyDisplayMetricsChanged() {
  observer_list_.Notify(&GpuSwitchingObserver::OnDisplayMetricsChanged);
}

}  // namespace ui
