// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_X_X11_SHM_IMAGE_POOL_H_
#define UI_BASE_X_X11_SHM_IMAGE_POOL_H_

#include <cstring>
#include <list>
#include <vector>

#include "base/component_export.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/base/x/x11_util.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/x/event.h"
#include "ui/gfx/x/shm.h"

namespace ui {

// Creates XImages backed by shared memory that will be shared with the X11
// server for processing.
class COMPONENT_EXPORT(UI_BASE_X) XShmImagePool : public x11::EventObserver {
 public:
  XShmImagePool(x11::Connection* connection,
                x11::Drawable drawable,
                x11::VisualId visual,
                int depth,
                std::size_t max_frames_pending,
                bool enable_multibuffering);

  XShmImagePool(const XShmImagePool&) = delete;
  XShmImagePool& operator=(const XShmImagePool&) = delete;

  ~XShmImagePool() override;

  bool Resize(const gfx::Size& pixel_size);

  // Is XSHM supported by the server and are the shared buffers ready for use?
  bool Ready();

  // Obtain state for the current frame.
  SkBitmap& CurrentBitmap();
  SkCanvas* CurrentCanvas();
  x11::Shm::Seg CurrentSegment();

  // Switch to the next cached frame.  CurrentBitmap() and CurrentImage() will
  // change to reflect the new frame.
  void SwapBuffers(base::OnceCallback<void(const gfx::Size&)> callback);

 protected:
  void DispatchShmCompletionEvent(x11::Shm::CompletionEvent event);

 private:
  friend class base::RefCountedThreadSafe<XShmImagePool>;

  struct FrameState {
    FrameState();
    ~FrameState();

    x11::Shm::Seg shmseg{};
    int shmid = 0;
    raw_ptr<void> shmaddr = nullptr;
    bool shmem_attached_to_server = false;
    SkBitmap bitmap;
    std::unique_ptr<SkCanvas> canvas;
  };

  struct SwapClosure {
    SwapClosure();
    ~SwapClosure();

    base::OnceClosure closure;
    x11::Shm::Seg shmseg{};
  };

  // x11::EventObserver:
  void OnEvent(const x11::Event& xev) override;

  void Cleanup();

  const raw_ptr<x11::Connection> connection_;
  const x11::Drawable drawable_;
  const x11::VisualId visual_;
  const int depth_;
  const bool enable_multibuffering_;

  bool ready_ = false;
  gfx::Size pixel_size_;
  std::size_t frame_bytes_ = 0;
  std::vector<FrameState> frame_states_;
  std::size_t current_frame_index_ = 0;
  std::list<SwapClosure> swap_closures_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace ui

#endif  // UI_BASE_X_X11_SHM_IMAGE_POOL_H_
