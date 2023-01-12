// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_LINUX_LINUX_UI_DELEGATE_H_
#define UI_LINUX_LINUX_UI_DELEGATE_H_

#include <cstdint>
#include <string>

#include "base/component_export.h"
#include "base/functional/callback_forward.h"

namespace gfx {
using AcceleratedWidget = uint32_t;
}

namespace ui {

enum class LinuxUiBackend {
  kStub,
  kX11,
  kWayland,
};

class COMPONENT_EXPORT(LINUX_UI) LinuxUiDelegate {
 public:
  static LinuxUiDelegate* GetInstance();

  LinuxUiDelegate();
  virtual ~LinuxUiDelegate();

  virtual LinuxUiBackend GetBackend() const = 0;

  // Only implemented on Wayland.
  virtual bool ExportWindowHandle(
      uint32_t parent_widget,
      base::OnceCallback<void(const std::string&)> callback);

  // Only implemented on X11.
  virtual void SetTransientWindowForParent(gfx::AcceleratedWidget parent,
                                           gfx::AcceleratedWidget transient);

  // Exports a prefixed, platform-dependent (X11 or Wayland) window handle for
  // an Aura window id, then calls the given callback with the handle. Returns
  // true on success.  |callback| may be run synchronously or asynchronously.
  virtual bool ExportWindowHandle(
      gfx::AcceleratedWidget window_id,
      base::OnceCallback<void(std::string)> callback) = 0;

 private:
  static LinuxUiDelegate* instance_;
};

}  // namespace ui

#endif  // UI_LINUX_LINUX_UI_DELEGATE_H_
