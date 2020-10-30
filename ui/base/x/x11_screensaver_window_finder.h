// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_X_X11_SCREENSAVER_WINDOW_FINDER_H_
#define UI_BASE_X_X11_SCREENSAVER_WINDOW_FINDER_H_

#include "base/component_export.h"
#include "base/macros.h"
#include "ui/base/x/x11_util.h"

namespace ui {

class COMPONENT_EXPORT(UI_BASE_X) ScreensaverWindowFinder
    : public ui::EnumerateWindowsDelegate {
 public:
  static bool ScreensaverWindowExists();

 protected:
  bool ShouldStopIterating(x11::Window window) override;

 private:
  ScreensaverWindowFinder();

  bool IsScreensaverWindow(x11::Window window) const;

  bool exists_;

  DISALLOW_COPY_AND_ASSIGN(ScreensaverWindowFinder);
};

}  // namespace ui

#endif  // UI_BASE_X_X11_SCREENSAVER_WINDOW_FINDER_H_
