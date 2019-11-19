// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gpu_switching_manager.h"

namespace ui {

// static
GpuSwitchingManager* GpuSwitchingManager::GetInstance() {
  return base::Singleton<GpuSwitchingManager>::get();
}

GpuSwitchingManager::GpuSwitchingManager() {}

GpuSwitchingManager::~GpuSwitchingManager() {}

void GpuSwitchingManager::AddObserver(GpuSwitchingObserver* observer) {
  observer_list_.AddObserver(observer);
}

void GpuSwitchingManager::RemoveObserver(GpuSwitchingObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

void GpuSwitchingManager::NotifyGpuSwitched(
    gl::GpuPreference active_gpu_heuristic) {
  for (GpuSwitchingObserver& observer : observer_list_)
    observer.OnGpuSwitched(active_gpu_heuristic);
}

}  // namespace ui
