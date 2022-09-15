// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_DISPLAY_MANAGER_H_
#define UI_GL_GL_DISPLAY_MANAGER_H_

#include <map>
#include <memory>
#include <vector>

#include "base/check.h"
#include "base/export_template.h"
#include "base/no_destructor.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "ui/gl/gl_display.h"
#include "ui/gl/gl_export.h"
#include "ui/gl/gpu_preference.h"

namespace gl {

template <typename GLDisplayPlatform>
class GLDisplayManager {
 public:
  // Getter for the singleton. This will return nullptr on failure.
  // This should only be called inside the ui/gl module. In component build,
  // calling this outside ui/gl module returns a different instance.
  static GLDisplayManager<GLDisplayPlatform>* GetInstance() {
    static base::NoDestructor<GLDisplayManager<GLDisplayPlatform>> instance;
    return instance.get();
  }

  // This should be called before calling GetDisplay(GpuPreference).
  // Otherwise kDefault GPU will be mapped to 0 instead of a valid
  // system_device_id.
  void SetGpuPreference(GpuPreference preference, uint64_t system_device_id) {
#if DCHECK_IS_ON()
    auto iter = gpu_preference_map_.find(preference);
    DCHECK(gpu_preference_map_.end() == iter);
#endif
    gpu_preference_map_[preference] = system_device_id;
  }

  GLDisplayManager(const GLDisplayManager&) = delete;
  GLDisplayManager& operator=(const GLDisplayManager&) = delete;

  GLDisplayPlatform* GetDisplay(uint64_t system_device_id) {
    base::AutoLock auto_lock(lock_);
    for (const auto& display : displays_) {
      if (display->system_device_id() == system_device_id) {
        return display.get();
      }
    }

    std::unique_ptr<GLDisplayPlatform> display(
        new GLDisplayPlatform(system_device_id));
    displays_.push_back(std::move(display));
    return displays_.back().get();
  }

  GLDisplayPlatform* GetDisplay(GpuPreference preference) {
    uint64_t system_device_id = 0;
    auto iter = gpu_preference_map_.find(preference);
    if (iter == gpu_preference_map_.end() &&
        preference != GpuPreference::kDefault) {
      // If kLowPower or kHighPerformance is queried but they are not set in the
      // map, default to the kDefault GPU.
      iter = gpu_preference_map_.find(GpuPreference::kDefault);
    }
    if (iter != gpu_preference_map_.end())
      system_device_id = iter->second;
    return GetDisplay(system_device_id);
  }

  bool IsEmpty() {
    base::AutoLock auto_lock(lock_);
    return displays_.empty();
  }

 private:
  friend class base::NoDestructor<GLDisplayManager<GLDisplayPlatform>>;
#if defined(USE_EGL)
  friend class GLDisplayManagerEGLTest;
#endif

  // Don't delete these functions for testing purpose.
  // Each test constructs a scoped GLDisplayManager directly.
  GLDisplayManager() = default;
  virtual ~GLDisplayManager() = default;

  mutable base::Lock lock_;
  std::vector<std::unique_ptr<GLDisplayPlatform>> displays_ GUARDED_BY(lock_);

  std::map<GpuPreference, uint64_t> gpu_preference_map_;
};

#if defined(USE_EGL)
using GLDisplayManagerEGL = GLDisplayManager<GLDisplayEGL>;

extern template class EXPORT_TEMPLATE_DECLARE(GL_EXPORT)
    GLDisplayManager<GLDisplayEGL>;
#endif

#if defined(USE_GLX)
using GLDisplayManagerX11 = GLDisplayManager<GLDisplayX11>;

extern template class EXPORT_TEMPLATE_DECLARE(GL_EXPORT)
    GLDisplayManager<GLDisplayX11>;
#endif

}  // namespace gl

#endif  // UI_GL_GL_DISPLAY_MANAGER_H_
