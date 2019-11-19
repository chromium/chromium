// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_COMPOSITOR_OBSERVER_H_
#define UI_COMPOSITOR_COMPOSITOR_OBSERVER_H_

#include "base/time/time.h"
#include "build/build_config.h"
#include "ui/compositor/compositor_export.h"

namespace gfx {
class Size;
}

namespace ui {

class Compositor;

// A compositor observer is notified when compositing completes.
class COMPOSITOR_EXPORT CompositorObserver {
 public:
  virtual ~CompositorObserver() = default;

  // A commit proxies information from the main thread to the compositor
  // thread. It typically happens when some state changes that will require a
  // composite. In the multi-threaded case, many commits may happen between
  // two successive composites. In the single-threaded, a single commit
  // between two composites (just before the composite as part of the
  // composite cycle). If the compositor is locked, it will not send this
  // this signal.
  virtual void OnCompositingDidCommit(Compositor* compositor) {}

  // Called when compositing started: it has taken all the layer changes into
  // account and has issued the graphics commands.
  virtual void OnCompositingStarted(Compositor* compositor,
                                    base::TimeTicks start_time) {}

  // Called when compositing completes: the present to screen has completed.
  virtual void OnCompositingEnded(Compositor* compositor) {}

  // Called when a child of the compositor is resizing.
  virtual void OnCompositingChildResizing(Compositor* compositor) {}

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  // Called when a swap with new size is completed.
  virtual void OnCompositingCompleteSwapWithNewSize(ui::Compositor* compositor,
                                                    const gfx::Size& size) {}
#endif  // defined(OS_LINUX)

  // Called at the top of the compositor's destructor, to give observers a
  // chance to remove themselves.
  virtual void OnCompositingShuttingDown(Compositor* compositor) {}
};

}  // namespace ui

#endif  // UI_COMPOSITOR_COMPOSITOR_OBSERVER_H_
