// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GPU_SWITCHING_MANAGER_H_
#define UI_GL_GPU_SWITCHING_MANAGER_H_

#include "base/memory/singleton.h"
#include "base/observer_list.h"
#include "build/build_config.h"
#include "ui/gl/gl_export.h"
#include "ui/gl/gpu_preference.h"
#include "ui/gl/gpu_switching_observer.h"

namespace ui {
// GpuSwitchingManager is not thread safe. It is running on the browser main
// thread in the browser and/or on the gpu main thread in the GPU process.
class GL_EXPORT GpuSwitchingManager {
 public:
  // Getter for the singleton. This will return NULL on failure.
  static GpuSwitchingManager* GetInstance();

  GpuSwitchingManager(const GpuSwitchingManager&) = delete;
  GpuSwitchingManager& operator=(const GpuSwitchingManager&) = delete;

  void AddObserver(GpuSwitchingObserver* observer);
  void RemoveObserver(GpuSwitchingObserver* observer);

  // Called when a monitor is plugged in. Only Windows is supported for now.
  void NotifyDisplayAdded();

  // Called when a monitor is unplugged.  Only Windows is supported for now.
  void NotifyDisplayRemoved();

  // Called when the display metrics changed.  Only Windows is supported for
  // now.
  void NotifyDisplayMetricsChanged();

 private:
  friend struct base::DefaultSingletonTraits<GpuSwitchingManager>;

  GpuSwitchingManager();
  virtual ~GpuSwitchingManager();

  base::ObserverList<GpuSwitchingObserver>::Unchecked observer_list_;
};

}  // namespace ui

#endif  // UI_GL_GPU_SWITCHING_MANAGER_H_
