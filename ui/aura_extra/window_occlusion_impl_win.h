// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_EXTRA_WINDOW_OCCLUSION_IMPL_WIN_H_
#define UI_AURA_EXTRA_WINDOW_OCCLUSION_IMPL_WIN_H_

#include <windows.h>
#include <winuser.h>

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/aura_extra/aura_extra_export.h"

namespace aura_extra {

// Delegate to get the native window bounds in pixels for a root aura::Window.
class WindowBoundsDelegate {
 public:
  WindowBoundsDelegate() {}
  virtual ~WindowBoundsDelegate() {}

  // Gets the bounds in pixels for |window|.
  virtual gfx::Rect GetBoundsInPixels(aura::WindowTreeHost* window) = 0;

  DISALLOW_COPY_AND_ASSIGN(WindowBoundsDelegate);
};

// Stores internal state during occlusion computation by
// ComputeNativeWindowOcclusionStatus().
class AURA_EXTRA_EXPORT WindowEvaluator {
 public:
  WindowEvaluator() {}
  // Called by NativeWindowIterator::Iterate and processes the metadata for a
  // single window. It is assumed that this is called in reverse z-order
  // (topmost window first, bottom window last). |is_relevant| describes if the
  // window is relevant to this calculation (it is visible and fully opaque),
  // |window_rect_in_pixels| is the bounds of the window in pixels, and |hwnd|
  // is the HWND of the window. Returns false if no more windows need to be
  // evaluated (this happens when all the desired occlusion states have been
  // determined), true otherwise.
  virtual bool EvaluateWindow(bool is_relevant,
                              const gfx::Rect& window_rect_in_pixels,
                              HWND hwnd) = 0;

  DISALLOW_COPY_AND_ASSIGN(WindowEvaluator);
};

// Interface to enumerate through all the native windows. Overriden in tests to
// avoid having to rely on the OS to enumerate native windows using
// EnumWindows().
class NativeWindowIterator {
 public:
  virtual ~NativeWindowIterator() {}

  // Enumerates through a collection of windows and applies |evaluator| to each
  // window. Enumeration is done from topmost to bottommost in the z-order, and
  // will stop once the evaluator returns false.
  virtual void Iterate(WindowEvaluator* evaluator) = 0;
};

class AURA_EXTRA_EXPORT WindowsDesktopWindowIterator
    : public NativeWindowIterator {
 public:
  WindowsDesktopWindowIterator();

  // NativeWindowIterator:
  void Iterate(WindowEvaluator* evaluator) override;

 private:
  // Runs |evaluator_| for |hwnd|. Returns TRUE if the evaluator should be run
  // again, FALSE otherwise.
  BOOL RunEvaluator(HWND hwnd);

  static BOOL CALLBACK EnumWindowsOcclusionCallback(HWND hwnd, LPARAM lParam);

  WindowEvaluator* evaluator_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(WindowsDesktopWindowIterator);
};

// Returns true if we are interested in |hwnd| for purposes of occlusion
// calculation. We are interested in |hwnd| if it is a window that is visible,
// opaque, and a simple rectangle. If we are interested in |hwnd|, stores the
// window rectangle in |rect|
bool AURA_EXTRA_EXPORT IsWindowVisibleAndFullyOpaque(HWND hwnd,
                                                     gfx::Rect* rect_in_pixels);

// Implementation of ComputeNativeWindowOcclusionStatus().
base::flat_map<aura::WindowTreeHost*, aura::Window::OcclusionState>
    AURA_EXTRA_EXPORT ComputeNativeWindowOcclusionStatusImpl(
        const std::vector<aura::WindowTreeHost*>& windows,
        std::unique_ptr<NativeWindowIterator> iterator,
        std::unique_ptr<WindowBoundsDelegate> bounds_delegate);

}  // namespace aura_extra

#endif  // UI_AURA_EXTRA_WINDOW_OCCLUSION_IMPL_WIN_H_
