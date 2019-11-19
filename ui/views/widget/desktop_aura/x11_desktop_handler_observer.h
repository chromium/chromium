// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_DESKTOP_AURA_X11_DESKTOP_HANDLER_OBSERVER_H_
#define UI_VIEWS_WIDGET_DESKTOP_AURA_X11_DESKTOP_HANDLER_OBSERVER_H_

#include <string>

#include "ui/views/views_export.h"

namespace views {

class VIEWS_EXPORT X11DesktopHandlerObserver {
 public:
  // Called when the (platform-specific) workspace ID changes to
  // |new_workspace|.
  virtual void OnWorkspaceChanged(const std::string& new_workspace) = 0;

 protected:
  virtual ~X11DesktopHandlerObserver() {}
};

}  // namespace views

#endif  // UI_VIEWS_WIDGET_DESKTOP_AURA_X11_DESKTOP_HANDLER_OBSERVER_H_
