// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_VSYNC_OBSERVER_H_
#define UI_GL_VSYNC_OBSERVER_H_

namespace gl {
class GL_EXPORT VSyncObserver {
 public:
  // Called on vsync thread.
  virtual void OnVSync(base::TimeTicks vsync_time,
                       base::TimeDelta interval) = 0;

 protected:
  virtual ~VSyncObserver() {}
};
}  // namespace gl

#endif  // UI_GL_VSYNC_OBSERVER_H_
