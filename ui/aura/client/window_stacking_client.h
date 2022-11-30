// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_CLIENT_WINDOW_STACKING_CLIENT_H_
#define UI_AURA_CLIENT_WINDOW_STACKING_CLIENT_H_


#include "ui/aura/aura_export.h"
#include "ui/aura/window.h"

namespace aura {
namespace client {

class AURA_EXPORT WindowStackingClient {
 public:
  // Invoked from the various Window stacking functions. Allows the
  // WindowStackingClient to alter the source, target and/or direction to stack.
  // Returns true if stacking should continue; false if the stacking should not
  // happen.
  virtual bool AdjustStacking(Window** child,
                              Window** target,
                              Window::StackDirection* direction) = 0;

 protected:
  virtual ~WindowStackingClient() {}
};

// Sets/gets the WindowStackingClient. This does *not* take ownership of
// |client|. It is assumed the caller will invoke SetWindowStackingClient(NULL)
// before deleting |client|.
AURA_EXPORT void SetWindowStackingClient(WindowStackingClient* client);
AURA_EXPORT WindowStackingClient* GetWindowStackingClient();

}  // namespace client
}  // namespace aura

#endif  // UI_AURA_CLIENT_WINDOW_STACKING_CLIENT_H_
