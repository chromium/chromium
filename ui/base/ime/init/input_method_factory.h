// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_INIT_INPUT_METHOD_FACTORY_H_
#define UI_BASE_IME_INIT_INPUT_METHOD_FACTORY_H_

#include <memory>

#include "base/component_export.h"
#include "ui/base/ime/init/input_method_initializer.h"
#include "ui/gfx/native_widget_types.h"

namespace ui {

class InputMethod;
class ImeKeyEventDispatcher;

// Creates a new instance of InputMethod and returns it.
COMPONENT_EXPORT(UI_BASE_IME_INIT)
std::unique_ptr<InputMethod> CreateInputMethod(
    ImeKeyEventDispatcher* ime_key_event_dispatcher,
    gfx::AcceleratedWidget widget);

// Makes CreateInputMethod return a MockInputMethod.
COMPONENT_EXPORT(UI_BASE_IME_INIT)
void SetUpInputMethodForTesting(InputMethod* input_method);

class COMPONENT_EXPORT(UI_BASE_IME_INIT) ScopedTestInputMethodFactory {
 public:
  ScopedTestInputMethodFactory();

  ScopedTestInputMethodFactory(const ScopedTestInputMethodFactory&) = delete;
  ScopedTestInputMethodFactory& operator=(const ScopedTestInputMethodFactory&) =
      delete;

  ~ScopedTestInputMethodFactory();
};

}  // namespace ui

#endif  // UI_BASE_IME_INIT_INPUT_METHOD_FACTORY_H_
