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
#include "ui/gl/gl_switches.h"
#include "ui/gl/gpu_preference.h"

namespace gl {

// TODO(344606399): Consider removing the templating since only the EGL display
// is used.
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

  // This should be called if display creation is failed on the specified
  // system_device_id display. This way, we will no longer attempt to use this
  // display, but instead use the default display.
  void RemoveGpuPreference(GpuPreference preference) {
    uint64_t system_device_id = GetSystemDeviceId(preference);
    for (auto iter = gpu_preference_map_.begin();
         iter != gpu_preference_map_.end();
         /* no increment */) {
      if (iter->second == system_device_id && gpu_preference_map_.size() > 1) {
        iter = gpu_preference_map_.erase(iter);
      } else {
        iter++;
      }
    }

    // Ensure that kDefault is always set if there is at least one other gpu
    // preference.
    if (!gpu_preference_map_.empty()) {
      auto iter = gpu_preference_map_.find(GpuPreference::kDefault);
      if (iter == gpu_preference_map_.end()) {
        gpu_preference_map_[GpuPreference::kDefault] =
            gpu_preference_map_.begin()->second;
      }
    }

    base::AutoLock auto_lock(lock_);
    for (size_t i = 0; i < displays_.size(); i++) {
      if (displays_[i]->system_device_id() == system_device_id) {
        displays_.erase(displays_.begin() + i);
        i--;
      }
    }
  }

  uint64_t GetSystemDeviceId(GpuPreference preference) {
    uint64_t system_device_id = 0;
    auto iter = gpu_preference_map_.find(preference);
    if (!SupportsEGLDualGPURendering() ||
        (iter == gpu_preference_map_.end() &&
         preference != GpuPreference::kDefault)) {
      // If kLowPower or kHighPerformance is queried but they are not set in the
      // map, default to the kDefault GPU.
      // Also do this if EGLDualGPURendering is not enabled.
      iter = gpu_preference_map_.find(GpuPreference::kDefault);
    }
    if (iter != gpu_preference_map_.end()) {
      system_device_id = iter->second;
    }
    return system_device_id;
  }

  GLDisplayManager(const GLDisplayManager&) = delete;
  GLDisplayManager& operator=(const GLDisplayManager&) = delete;

  bool IsEmpty() {
    base::AutoLock auto_lock(lock_);
    return displays_.empty();
  }

  void OverrideEGLDualGPURenderingSupportForTests(bool value) {
    override_egl_dual_gpu_rendering_support_for_tests_ = value;
  }

  bool SupportsEGLDualGPURendering() {
    return features::SupportsEGLDualGPURendering() ||
           override_egl_dual_gpu_rendering_support_for_tests_;
  }

  GLDisplayPlatform* GetDisplay(GpuPreference preference,
                                gl::DisplayKey display_key) {
    return GetDisplay(GetSystemDeviceId(preference), display_key);
  }

  GLDisplayPlatform* GetDisplay(GpuPreference preference) {
    return GetDisplay(GetSystemDeviceId(preference), gl::DisplayKey::kDefault);
  }

 private:
  friend class base::NoDestructor<GLDisplayManager<GLDisplayPlatform>>;
  friend class GLDisplayManagerEGLTest;

  // Don't delete these functions for testing purpose.
  // Each test constructs a scoped GLDisplayManager directly.
  GLDisplayManager() = default;
  virtual ~GLDisplayManager() = default;

  GLDisplayPlatform* GetDisplay(uint64_t system_device_id,
                                gl::DisplayKey display_key) {
    base::AutoLock auto_lock(lock_);
    for (const auto& display : displays_) {
      if (display->system_device_id() == system_device_id &&
          display->display_key() == display_key) {
        return display.get();
      }
    }

    std::unique_ptr<GLDisplayPlatform> display(
        new GLDisplayPlatform(system_device_id, display_key));
    displays_.push_back(std::move(display));
    return displays_.back().get();
  }

  mutable base::Lock lock_;
  std::vector<std::unique_ptr<GLDisplayPlatform>> displays_ GUARDED_BY(lock_);

  std::map<GpuPreference, uint64_t> gpu_preference_map_;

  bool override_egl_dual_gpu_rendering_support_for_tests_ = false;
};

using GLDisplayManagerEGL = GLDisplayManager<GLDisplayEGL>;

extern template class EXPORT_TEMPLATE_DECLARE(GL_EXPORT)
    GLDisplayManager<GLDisplayEGL>;

}  // namespace gl

#endif  // UI_GL_GL_DISPLAY_MANAGER_H_
