// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_IMPLEMENTATION_WRAPPER_H_
#define UI_GL_GL_IMPLEMENTATION_WRAPPER_H_

#include <memory>

#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "ui/gl/gl_switches.h"

#define GL_IMPL_WRAPPER_TYPE(name) \
  GLImplementationWrapper<name##Api, Trace##name##Api, Log##name##Api>

namespace gl {

// Wraps a GLApi with its tracing and logging variants when the corresponding
// command line flags are passed.
template <class GLImplApi, class GLTraceImplApi, class GLLogImplApi>
class GLImplementationWrapper {
 public:
  GLImplementationWrapper(std::unique_ptr<GLImplApi> real_gl)
      : real_gl_(std::move(real_gl)) {
    gl_api_ = real_gl_.get();

    static bool enable_tracing =
        base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kEnableGPUServiceTracing);
    if (enable_tracing) {
      trace_gl_ = std::make_unique<GLTraceImplApi>(gl_api_);
      gl_api_ = trace_gl_.get();
    }

    static bool enable_logging =
        base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kEnableGPUServiceLogging);
    if (enable_logging) {
      log_gl_ = std::make_unique<GLLogImplApi>(gl_api_);
      gl_api_ = log_gl_.get();
    }
  }

  GLImplementationWrapper(const GLImplementationWrapper&) = delete;
  GLImplementationWrapper& operator=(const GLImplementationWrapper&) = delete;

  ~GLImplementationWrapper() = default;

  GLImplApi* api() { return gl_api_; }

 private:
  std::unique_ptr<GLImplApi> real_gl_;
  std::unique_ptr<GLTraceImplApi> trace_gl_;
  std::unique_ptr<GLLogImplApi> log_gl_;
  raw_ptr<GLImplApi> gl_api_ = nullptr;
};

}  // namespace gl

#endif  // UI_GL_GL_IMPLEMENTATION_WRAPPER_H_
