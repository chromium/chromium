// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_CLIENT_FOCUS_CLIENT_H_
#define UI_AURA_CLIENT_FOCUS_CLIENT_H_

#include "ui/aura/aura_export.h"
#include "ui/aura/window.h"

namespace aura {
namespace client {
class FocusChangeObserver;

// An interface implemented by an object that manages window focus.
class AURA_EXPORT FocusClient {
 public:
  virtual ~FocusClient() {}

  virtual void AddObserver(FocusChangeObserver* observer) = 0;
  virtual void RemoveObserver(FocusChangeObserver* observer) = 0;

  // Focuses |window|. Passing NULL clears focus.
  virtual void FocusWindow(Window* window) = 0;

  // Sets focus to |window| if it's within the active window. Not intended as a
  // general purpose API, use FocusWindow() instead.
  virtual void ResetFocusWithinActiveWindow(Window* window) = 0;

  // Retrieves the focused window, or NULL if there is none.
  virtual Window* GetFocusedWindow() = 0;
};

// Sets/Gets the focus client on the root Window.
AURA_EXPORT void SetFocusClient(Window* root_window, FocusClient* client);
AURA_EXPORT FocusClient* GetFocusClient(Window* window);
AURA_EXPORT FocusClient* GetFocusClient(const Window* window);

}  // namespace clients
}  // namespace aura

#endif  // UI_AURA_CLIENT_FOCUS_CLIENT_H_
