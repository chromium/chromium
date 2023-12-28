// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_TEST_BEGIN_MAIN_FRAME_WAITER_H_
#define UI_COMPOSITOR_TEST_BEGIN_MAIN_FRAME_WAITER_H_

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "ui/compositor/compositor_observer.h"

namespace ui {

class Compositor;

class BeginMainFrameWaiter : public CompositorObserver {
 public:
  explicit BeginMainFrameWaiter(Compositor* compositor);

  BeginMainFrameWaiter(const BeginMainFrameWaiter&) = delete;
  BeginMainFrameWaiter& operator=(const BeginMainFrameWaiter&) = delete;

  ~BeginMainFrameWaiter() override;

  // ui::CompositorObserver
  void OnDidBeginMainFrame(Compositor* compositor) override;

  // True if BeginMainFrame has been received.
  bool begin_main_frame_received() const { return begin_main_frame_received_; }

  void Wait();

 private:
  raw_ptr<Compositor> compositor_;
  bool begin_main_frame_received_ = false;
  base::RunLoop run_loop_;
};

}  // namespace ui

#endif  // UI_COMPOSITOR_TEST_BEGIN_MAIN_FRAME_WAITER_H_
