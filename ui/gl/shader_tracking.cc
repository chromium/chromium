// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/shader_tracking.h"

#include "base/check.h"
#include "ui/gl/gl_switches.h"

namespace gl {

// static
ShaderTracking* ShaderTracking::GetInstance() {
#if defined(OS_WIN)
  // Shaders can only be reliably retrieved with ANGLE backend. Therefore,
  // limit to Windows platform only.
  static bool enabled =
      base::FeatureList::IsEnabled(features::kTrackCurrentShaders);
  if (enabled) {
    static base::NoDestructor<ShaderTracking> instance;
    return instance.get();
  }
#endif  // OS_WIN
  return nullptr;
}

void ShaderTracking::GetShaders(std::string* shader0, std::string* shader1) {
  DCHECK(shader0 && shader1);
  base::AutoLock auto_lock(lock_);
  *shader0 = shaders_[0];
  *shader1 = shaders_[1];
}

void ShaderTracking::SetShaders(const char* shader0, const char* shader1) {
  base::AutoLock auto_lock(lock_);
  shaders_[0] = shader0 ? shader0 : "";
  shaders_[1] = shader1 ? shader1 : "";
}

}  // namespace gl
