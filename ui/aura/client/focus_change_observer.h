// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_CLIENT_FOCUS_CHANGE_OBSERVER_H_
#define UI_AURA_CLIENT_FOCUS_CHANGE_OBSERVER_H_

#include "ui/aura/aura_export.h"

namespace aura {
class Window;
namespace client {

// TODO(beng): this interface will be OBSOLETE by FocusChangeEvent.
class AURA_EXPORT FocusChangeObserver {
 public:
  // Called when focus moves from |lost_focus| to |gained_focus|.
  virtual void OnWindowFocused(Window* gained_focus, Window* lost_focus) = 0;

 protected:
  virtual ~FocusChangeObserver() {}
};

AURA_EXPORT FocusChangeObserver* GetFocusChangeObserver(Window* window);
AURA_EXPORT void SetFocusChangeObserver(
    Window* window,
    FocusChangeObserver* focus_change_observer);


}  // namespace client
}  // namespace aura

#endif  // UI_AURA_CLIENT_FOCUS_CHANGE_OBSERVER_H_
