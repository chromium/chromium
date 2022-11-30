// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_INIT_INPUT_METHOD_INITIALIZER_H_
#define UI_BASE_IME_INIT_INPUT_METHOD_INITIALIZER_H_

#include "base/component_export.h"

namespace ui {

// Initializes thread-local resources for input method. This function should be
// called in the UI thread before input method is used.
COMPONENT_EXPORT(UI_BASE_IME_INIT) void InitializeInputMethod();

// Shutdown thread-local resources for input method. This function should be
// called in the UI thread after input method is used.
COMPONENT_EXPORT(UI_BASE_IME_INIT) void ShutdownInputMethod();

// Initializes thread-local resources for input method. This function is
// intended to be called from Setup function of unit tests.
COMPONENT_EXPORT(UI_BASE_IME_INIT) void InitializeInputMethodForTesting();

// Initializes thread-local resources for input method. This function is
// intended to be called from TearDown function of unit tests.
COMPONENT_EXPORT(UI_BASE_IME_INIT) void ShutdownInputMethodForTesting();

}  // namespace ui

#endif  // UI_BASE_IME_INIT_INPUT_METHOD_INITIALIZER_H_
