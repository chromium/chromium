// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_DEMO_RENDERER_H_
#define UI_OZONE_DEMO_RENDERER_H_

namespace ui {

class Renderer {
 public:
  virtual ~Renderer() {}

  virtual bool Initialize() = 0;
};

}  // namespace ui

#endif  // UI_OZONE_DEMO_RENDERER_H_
