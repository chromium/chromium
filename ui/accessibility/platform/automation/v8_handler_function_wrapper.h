// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_AUTOMATION_V8_HANDLER_FUNCTION_WRAPPER_H_
#define UI_ACCESSIBILITY_PLATFORM_AUTOMATION_V8_HANDLER_FUNCTION_WRAPPER_H_

#include "base/component_export.h"
#include "base/memory/ref_counted.h"
#include "v8/include/v8-function-callback.h"
#include "v8/include/v8-value.h"

namespace gin {
class Arguments;
}  // namespace gin

namespace ui {

// Virtual class which can wrap a handler function passed to V8 either with
// Gin or V8 function callbacks. This enables some AutomationV8Routers to
// use V8 bindings and others to use gin.
class COMPONENT_EXPORT(AX_PLATFORM) V8HandlerFunctionWrapper
    : public base::RefCountedThreadSafe<V8HandlerFunctionWrapper> {
 public:
  V8HandlerFunctionWrapper() = default;

  // Executes the wrapped HandlerFunction with the given Gin arguments.
  // Maybe be used to bind using gin::CreateFunctionTemplate.
  virtual void Run(gin::Arguments* args) = 0;

  // Executes the wrapped HandlerFunction with the given V8 arguments.
  // Maybe used to bind using v8::FunctionTemplate::New.
  void RunV8(const v8::FunctionCallbackInfo<v8::Value>& args);

 protected:
  // Protected destructor allows base classes to override while ensuring
  // that only RefCountedThreadSafe can destroy these objects.
  virtual ~V8HandlerFunctionWrapper() = default;

 private:
  friend class base::RefCountedThreadSafe<V8HandlerFunctionWrapper>;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_AUTOMATION_V8_HANDLER_FUNCTION_WRAPPER_H_
