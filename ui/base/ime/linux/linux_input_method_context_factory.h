// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_LINUX_LINUX_INPUT_METHOD_CONTEXT_FACTORY_H_
#define UI_BASE_IME_LINUX_LINUX_INPUT_METHOD_CONTEXT_FACTORY_H_

#include <memory>

#include "base/component_export.h"
#include "base/functional/callback_forward.h"

namespace ui {

class LinuxInputMethodContext;
class LinuxInputMethodContextDelegate;

using LinuxInputMethodContextFactory =
    base::RepeatingCallback<std::unique_ptr<ui::LinuxInputMethodContext>(
        LinuxInputMethodContextDelegate*)>;

// Callers may set the returned reference to set the factory.
COMPONENT_EXPORT(UI_BASE_IME_LINUX)
LinuxInputMethodContextFactory& GetInputMethodContextFactoryForOzone();

// The test context factory has higher precedence than the ozone factory.
COMPONENT_EXPORT(UI_BASE_IME_LINUX)
LinuxInputMethodContextFactory& GetInputMethodContextFactoryForTest();

// Returns a platform specific input method context.
COMPONENT_EXPORT(UI_BASE_IME_LINUX)
std::unique_ptr<LinuxInputMethodContext> CreateLinuxInputMethodContext(
    LinuxInputMethodContextDelegate* delegate);

}  // namespace ui

#endif  // UI_BASE_IME_LINUX_LINUX_INPUT_METHOD_CONTEXT_FACTORY_H_
