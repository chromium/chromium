// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_PLATFORM_WINDOW_EXTENSIONS_BEGIN_FRAME_SOURCE_EXTENSION_H_
#define UI_PLATFORM_WINDOW_EXTENSIONS_BEGIN_FRAME_SOURCE_EXTENSION_H_

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/time/time.h"

namespace ui {

class PlatformWindow;

// A viz/mojo begin frame source driver that can be implemented by a
// PlatformWindow.
class COMPONENT_EXPORT(PLATFORM_WINDOW) BeginFrameSourceExtension {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;

    virtual void OnBeginFrame(
        base::TimeTicks frame_time,
        base::TimeTicks deadline,
        base::TimeDelta interval,
        base::OnceCallback<void(bool has_damage)> ack_callback) = 0;
  };

  virtual ~BeginFrameSourceExtension();

  virtual void SetDelegate(Delegate* delegate) = 0;
  virtual void SetNeedsBeginFrame(bool needs) = 0;
  virtual void SetPreferredInterval(base::TimeDelta interval) = 0;

 protected:
  void SetBeginFrameSourceExtension(PlatformWindow* window,
                                    BeginFrameSourceExtension* source);
};

COMPONENT_EXPORT(PLATFORM_WINDOW)
BeginFrameSourceExtension* GetBeginFrameSourceExtension(
    const PlatformWindow& window);

}  // namespace ui

#endif  // UI_PLATFORM_WINDOW_EXTENSIONS_BEGIN_FRAME_SOURCE_EXTENSION_H_
