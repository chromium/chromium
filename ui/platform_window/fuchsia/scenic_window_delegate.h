// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_PLATFORM_WINDOW_FUCHSIA_SCENIC_WINDOW_DELEGATE_H_
#define UI_PLATFORM_WINDOW_FUCHSIA_SCENIC_WINDOW_DELEGATE_H_

namespace ui {

// Interface use by ScenicWindow to notify the client about Scenic-specific
// events.
class COMPONENT_EXPORT(PLATFORM_WINDOW) ScenicWindowDelegate {
 public:
  // Called to notify about logical pixel scale changes.
  virtual void OnScenicPixelScale(PlatformWindow* window, float scale) = 0;

 protected:
  virtual ~ScenicWindowDelegate() {}
};

}  // namespace ui

#endif  // UI_PLATFORM_WINDOW_FUCHSIA_SCENIC_WINDOW_DELEGATE_H_
