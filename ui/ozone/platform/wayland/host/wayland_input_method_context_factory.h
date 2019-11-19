// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_INPUT_METHOD_CONTEXT_FACTORY_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_INPUT_METHOD_CONTEXT_FACTORY_H_

#include <memory>

#include "base/macros.h"
#include "ui/base/ime/linux/linux_input_method_context_factory.h"

namespace ui {

class WaylandConnection;
class WaylandInputMethodContext;

class WaylandInputMethodContextFactory : public LinuxInputMethodContextFactory {
 public:
  explicit WaylandInputMethodContextFactory(WaylandConnection* connection);
  ~WaylandInputMethodContextFactory() override;

  std::unique_ptr<LinuxInputMethodContext> CreateInputMethodContext(
      LinuxInputMethodContextDelegate* delegate,
      bool is_simple) const override;

  // Exposed for unit tests but also called by CreateInputMethodContext
  std::unique_ptr<WaylandInputMethodContext> CreateWaylandInputMethodContext(
      ui::LinuxInputMethodContextDelegate* delegate,
      bool is_simple) const;

 private:
  WaylandConnection* connection_;

  DISALLOW_COPY_AND_ASSIGN(WaylandInputMethodContextFactory);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_INPUT_METHOD_CONTEXT_FACTORY_H_
