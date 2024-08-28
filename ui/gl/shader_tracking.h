// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_SHADER_TRACKING_H_
#define UI_GL_SHADER_TRACKING_H_

#include <string>

#include "base/no_destructor.h"
#include "base/synchronization/lock.h"
#include "build/build_config.h"
#include "ui/gl/gl_export.h"

namespace gl {

class GL_EXPORT ShaderTracking {
 public:
  static ShaderTracking* GetInstance();

  ShaderTracking(const ShaderTracking&) = delete;
  ShaderTracking& operator=(const ShaderTracking&) = delete;

  static const size_t kMaxShaderSize = 1024;

  void GetShaders(std::string* shader0, std::string* shader1);

  void SetShaders(const char* shader0, const char* shader1);

 private:
  friend base::NoDestructor<ShaderTracking>;

  ShaderTracking() {}
  ~ShaderTracking() {}

  mutable base::Lock lock_;
  std::string shader0_;
  std::string shader1_;
};

}  // namespace gl

#endif  // UI_GL_SHADER_TRACKING_H_
