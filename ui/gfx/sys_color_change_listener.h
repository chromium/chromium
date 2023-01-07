// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_SYS_COLOR_CHANGE_LISTENER_H_
#define UI_GFX_SYS_COLOR_CHANGE_LISTENER_H_

#include "base/memory/raw_ptr.h"
#include "ui/gfx/gfx_export.h"

namespace gfx {

// Interface for classes that want to listen to system color changes.
class GFX_EXPORT SysColorChangeListener {
 public:
  virtual void OnSysColorChange() = 0;

 protected:
  virtual ~SysColorChangeListener() {}
};

// Create an instance of this class in any object that wants to listen
// for system color changes.
class GFX_EXPORT ScopedSysColorChangeListener {
 public:
  explicit ScopedSysColorChangeListener(SysColorChangeListener* listener);

  ScopedSysColorChangeListener(const ScopedSysColorChangeListener&) = delete;
  ScopedSysColorChangeListener& operator=(const ScopedSysColorChangeListener&) =
      delete;

  ~ScopedSysColorChangeListener();

 private:
  raw_ptr<SysColorChangeListener> listener_;
};

}  // namespace gfx;

#endif  // UI_GFX_SYS_COLOR_CHANGE_LISTENER_H_
