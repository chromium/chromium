// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_LINUX_LINUX_INPUT_METHOD_CONTEXT_FACTORY_H_
#define UI_BASE_IME_LINUX_LINUX_INPUT_METHOD_CONTEXT_FACTORY_H_

#include <memory>

#include "base/component_export.h"

namespace ui {

class LinuxInputMethodContext;
class LinuxInputMethodContextDelegate;

// An interface that lets different Linux platforms override the
// CreateInputMethodContext function declared here to return native input method
// contexts.
class COMPONENT_EXPORT(UI_BASE_IME_LINUX) LinuxInputMethodContextFactory {
 public:
  // Returns the current active factory or NULL.
  static const LinuxInputMethodContextFactory* instance();

  // Sets the dynamically loaded singleton that creates an input method context.
  // This pointer is not owned, and if this method is called a second time,
  // the first instance is not deleted.
  static void SetInstance(const LinuxInputMethodContextFactory* instance);

  virtual ~LinuxInputMethodContextFactory() {}

  // Returns a native input method context.
  virtual std::unique_ptr<LinuxInputMethodContext> CreateInputMethodContext(
      LinuxInputMethodContextDelegate* delegate,
      bool is_simple) const = 0;
};

}  // namespace ui

#endif  // UI_BASE_IME_LINUX_LINUX_INPUT_METHOD_CONTEXT_FACTORY_H_
