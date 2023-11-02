// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_CLIENT_VISIBILITY_CLIENT_H_
#define UI_AURA_CLIENT_VISIBILITY_CLIENT_H_

#include "ui/aura/aura_export.h"

namespace aura {
class Window;
namespace client {

// An interface implemented by an object that manages the visibility of Windows'
// layers as Window visibility changes.
class AURA_EXPORT VisibilityClient {
 public:
  // Called when |window|'s visibility is changing to |visible|. The implementor
  // can perform additional actions before reflecting the visibility change on
  // the underlying layer.
  virtual void UpdateLayerVisibility(Window* window, bool visible) = 0;

 protected:
  virtual ~VisibilityClient() {}
};

// Sets the VisibilityClient on the Window.
AURA_EXPORT void SetVisibilityClient(Window* window, VisibilityClient* client);

// Gets the VisibilityClient for the window. This will crawl up |window|'s
// hierarchy until it finds one.
AURA_EXPORT VisibilityClient* GetVisibilityClient(Window* window);

}  // namespace clients
}  // namespace aura

#endif  // UI_AURA_CLIENT_VISIBILITY_CLIENT_H_
