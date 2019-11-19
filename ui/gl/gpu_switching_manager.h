// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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

class GL_EXPORT GpuSwitchingManager {
 public:
  // Getter for the singleton. This will return NULL on failure.
  static GpuSwitchingManager* GetInstance();

  void AddObserver(GpuSwitchingObserver* observer);
  void RemoveObserver(GpuSwitchingObserver* observer);

  // Called when a GPU switch is noticed by the system. In the browser process
  // this is occurs as a result of a system observer. In the GPU process, this
  // occurs as a result of an IPC from the browser. The system observer is kept
  // in the browser process only so that any workarounds or blacklisting can
  // be applied there.
  //
  // The GpuPreference argument is a heuristic indicating whether the
  // system is known to be on the low-power or high-performance GPU.
  // If this heuristic fails, then kDefault is passed as argument.
  void NotifyGpuSwitched(gl::GpuPreference active_gpu_heuristic);

 private:
  friend struct base::DefaultSingletonTraits<GpuSwitchingManager>;

  GpuSwitchingManager();
  virtual ~GpuSwitchingManager();

  base::ObserverList<GpuSwitchingObserver>::Unchecked observer_list_;

  DISALLOW_COPY_AND_ASSIGN(GpuSwitchingManager);
};

}  // namespace ui

#endif  // UI_GL_GPU_SWITCHING_MANAGER_H_
