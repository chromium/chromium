// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WINDOW_WINDOW_RESOURCES_H_
#define UI_VIEWS_WINDOW_WINDOW_RESOURCES_H_

namespace gfx {
class ImageSkia;
}

namespace views {

using FramePartImage = int;

///////////////////////////////////////////////////////////////////////////////
// WindowResources
//
//  An interface implemented by an object providing images to render the
//  contents of a window frame. The Window may swap in different
//  implementations of this interface to render different modes. The definition
//  of FramePartImage depends on the implementation.
//
class WindowResources {
 public:
  virtual ~WindowResources() = default;

  virtual gfx::ImageSkia* GetPartImage(FramePartImage part) const = 0;
};

}  // namespace views

#endif  // UI_VIEWS_WINDOW_WINDOW_RESOURCES_H_
