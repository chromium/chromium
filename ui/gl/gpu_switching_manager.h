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

  // Called when a GPU switch is noticed by the system. In the browser process
  // this is occurs as a result of a system observer. In the GPU process, this
  // occurs as a result of an IPC from the browser. The system observer is kept
  // in the browser process only so that any workarounds or blocklisting can
  // be applied there.
  //
  // The GpuPreference argument is a heuristic indicating whether the
  // system is known to be on the low-power or high-performance GPU.
  // If this heuristic fails, then kDefault is passed as argument.
  // Only Mac is supported for now.
  void NotifyGpuSwitched(gl::GpuPreference active_gpu_heuristic);

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
