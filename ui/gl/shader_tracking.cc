// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/shader_tracking.h"

#include "base/check.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "ui/gl/gl_switches.h"

namespace gl {

// static
ShaderTracking* ShaderTracking::GetInstance() {
#if BUILDFLAG(IS_WIN)
  // Shaders can only be reliably retrieved with ANGLE backend. Therefore,
  // limit to Windows platform only.
  static bool enabled =
      base::FeatureList::IsEnabled(features::kTrackCurrentShaders);
  if (enabled) {
    static base::NoDestructor<ShaderTracking> instance;
    return instance.get();
  }
#endif  // BUILDFLAG(IS_WIN)
  return nullptr;
}

void ShaderTracking::GetShaders(std::string* shader0, std::string* shader1) {
  DCHECK(shader0 && shader1);
  base::AutoLock auto_lock(lock_);
  *shader0 = shader0_;
  *shader1 = shader1_;
}

void ShaderTracking::SetShaders(const char* shader0, const char* shader1) {
  base::AutoLock auto_lock(lock_);
  shader0_ = shader0 ? shader0 : "";
  shader1_ = shader1 ? shader1 : "";
}

}  // namespace gl
