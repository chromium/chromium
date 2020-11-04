// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura_extra/window_occlusion_impl_win.h"

#include "base/win/scoped_gdi_object.h"
#include "ui/aura/window_tree_host.h"
#include "ui/gfx/geometry/rect.h"

namespace aura_extra {

namespace {

// Determines the occlusion status of each aura::Window in |windows_of_interest|
// passed to the constructor. Evaluates a window by subtracting its bounds from
// every window beneath it in the z-order.
class WindowEvaluatorImpl : public WindowEvaluator {
 public:
  // |windows_of_interest| are the aura::WindowTreeHost's  whose occlusion
  // status are being calculated. |bounds_delegate| is used to obtain the bounds
  // in pixels of each root aura::Window in |windows_of_interest|.
  WindowEvaluatorImpl(
      const std::vector<aura::WindowTreeHost*>& windows_of_interest,
      std::unique_ptr<WindowBoundsDelegate> bounds_delegate);
  ~WindowEvaluatorImpl();

  // WindowEvaluator.
  bool EvaluateWindow(bool is_relevant,
                      const gfx::Rect& window_rect_in_pixels,
                      HWND hwnd) override;

  // Returns whether there was at least one visible root aura::Window passed to
  // ComputeNativeWindowOcclusionStatus().
  bool HasAtLeastOneVisibleWindow() const {
    return !unoccluded_regions_.empty();
  }

  // Called once the occlusion computation is done. Returns |occlusion_states_|
  base::flat_map<aura::WindowTreeHost*, aura::Window::OcclusionState>
  TakeResult();

 private:
  using WindowRegionPair = std::pair<aura::WindowTreeHost*, SkRegion>;

  // Stores intermediate values for the unoccluded regions of an aura::Window in
  // pixels. Once an aura::Window::OcclusionState is determined for a root
  // aura::Window, that aura::Window is removed from |unoccluded_regions_| and
  // added to |occlusion_states_| with the computed
  // aura::Window::OcclusionState.
  std::vector<WindowRegionPair> unoccluded_regions_;

  // Stores the final aura::Window::OcclusionState for each root
  // aura::WindowTreeHost that is passed to
  // ComputeNativeWindowOcclusionStatus().
  base::flat_map<aura::WindowTreeHost*, aura::Window::OcclusionState>
      occlusion_states_;

  DISALLOW_COPY_AND_ASSIGN(WindowEvaluatorImpl);
};

WindowEvaluatorImpl::WindowEvaluatorImpl(
    const std::vector<aura::WindowTreeHost*>& windows_of_interest,
    std::unique_ptr<WindowBoundsDelegate> bounds_delegate) {
  for (aura::WindowTreeHost* window : windows_of_interest) {
    // If the window isn't visible at this time, it is
    // aura::Window::OcclusionState::HIDDEN.
    if (!window->window()->IsVisible() ||
        IsIconic(window->GetAcceleratedWidget())) {
      occlusion_states_[window] = aura::Window::OcclusionState::HIDDEN;
      continue;
    }

    gfx::Rect window_rect_in_pixels =
        bounds_delegate->GetBoundsInPixels(window);

    SkRegion window_region(SkIRect::MakeXYWH(
        window_rect_in_pixels.x(), window_rect_in_pixels.y(),
        window_rect_in_pixels.width(), window_rect_in_pixels.height()));

    unoccluded_regions_.emplace_back(window, window_region);
  }
}

WindowEvaluatorImpl::~WindowEvaluatorImpl() = default;

bool WindowEvaluatorImpl::EvaluateWindow(bool is_relevant,
                                         const gfx::Rect& window_rect_in_pixels,
                                         HWND hwnd) {
  // Loop through |unoccluded_regions_| and determine how |hwnd| affects each
  // root window with respect to occlusion.
  for (WindowRegionPair& root_window_pair : unoccluded_regions_) {
    HWND window_hwnd = root_window_pair.first->GetAcceleratedWidget();

    // The EnumWindows callbacks have reached this window in the Z-order
    // (EnumWindows goes from front to back). This window must be visible
    // because we did not discover that it was completely occluded earlier.
    if (hwnd == window_hwnd) {
      occlusion_states_[root_window_pair.first] =
          aura::Window::OcclusionState::VISIBLE;
      // Set the unoccluded region for this window to empty as a signal that the
      // occlusion computation is complete.
      root_window_pair.second.setEmpty();
      continue;
    }

    // |hwnd| is not taken into account for occlusion computations, move on.
    if (!is_relevant)
      continue;

    // Current occlusion state for this window cannot be determined yet. The
    // EnumWindows callbacks are currently above this window in the Z-order.
    // Subtract the other windows bounding rectangle from this window's
    // unoccluded region if the two regions intersect.
    SkRegion window_region(SkIRect::MakeXYWH(
        window_rect_in_pixels.x(), window_rect_in_pixels.y(),
        window_rect_in_pixels.width(), window_rect_in_pixels.height()));

    if (root_window_pair.second.intersects(window_region)) {
      root_window_pair.second.op(window_region, SkRegion::kDifference_Op);

      if (root_window_pair.second.isEmpty()) {
        occlusion_states_[root_window_pair.first] =
            aura::Window::OcclusionState::OCCLUDED;
      }
    }
  }

  // Occlusion computation is done for windows with an empty region in
  // |unoccluded_regions_|. If the window is visible, the region is set to empty
  // explicitly. If it is occluded, the region is implicitly empty.
  base::EraseIf(unoccluded_regions_, [](const WindowRegionPair& element) {
    return element.second.isEmpty();
  });

  // If |unoccluded_regions_| is empty, the occlusion calculation is complete.
  // So, we return false to signal to EnumWindows to stop enumerating.
  // Otherwise, we want EnumWindows to continue, and return true.
  return !unoccluded_regions_.empty();
}

base::flat_map<aura::WindowTreeHost*, aura::Window::OcclusionState>
WindowEvaluatorImpl::TakeResult() {
  return std::move(occlusion_states_);
}

}  // namespace

WindowsDesktopWindowIterator::WindowsDesktopWindowIterator() = default;

void WindowsDesktopWindowIterator::Iterate(WindowEvaluator* evaluator) {
  evaluator_ = evaluator;
  EnumWindows(&EnumWindowsOcclusionCallback, reinterpret_cast<LPARAM>(this));
}

BOOL WindowsDesktopWindowIterator::RunEvaluator(HWND hwnd) {
  gfx::Rect window_rect;
  bool is_relevant = IsWindowVisibleAndFullyOpaque(hwnd, &window_rect);
  return evaluator_->EvaluateWindow(is_relevant, window_rect, hwnd);
}

// static
BOOL CALLBACK
WindowsDesktopWindowIterator::EnumWindowsOcclusionCallback(HWND hwnd,
                                                           LPARAM lParam) {
  WindowsDesktopWindowIterator* iterator =
      reinterpret_cast<WindowsDesktopWindowIterator*>(lParam);
  return iterator->RunEvaluator(hwnd);
}

base::flat_map<aura::WindowTreeHost*, aura::Window::OcclusionState>
ComputeNativeWindowOcclusionStatusImpl(
    const std::vector<aura::WindowTreeHost*>& windows,
    std::unique_ptr<NativeWindowIterator> iterator,
    std::unique_ptr<WindowBoundsDelegate> bounds_delegate) {
  // Time to execute this method, according to 28 days of Stable data
  // ending on June 15, 2020:
  //
  //  50th percentile: 156 us
  //  75th percentile: 273 us
  //  95th percentile: 647 us
  //  99th percentile: 1939 us
  //  99.5th percentile: 5592 us

  WindowEvaluatorImpl window_evaluator(windows, std::move(bounds_delegate));

  // Only compute occlusion if there was at least one window that is visible.
  if (window_evaluator.HasAtLeastOneVisibleWindow())
    iterator->Iterate(&window_evaluator);

  return window_evaluator.TakeResult();
}

bool IsWindowVisibleAndFullyOpaque(HWND hwnd, gfx::Rect* rect_in_pixels) {
  // Filter out invalid hwnds.
  if (!IsWindow(hwnd))
    return false;

  // Filter out windows that are not “visible”.
  if (!IsWindowVisible(hwnd))
    return false;

  // Filter out minimized windows.
  if (IsIconic(hwnd))
    return false;

  LONG ex_styles = GetWindowLong(hwnd, GWL_EXSTYLE);

  // Filter out “transparent” windows, windows where the mouse clicks fall
  // through them.
  if (ex_styles & WS_EX_TRANSPARENT)
    return false;

  // Filter out “tool windows”, which are floating windows that do not appear on
  // the taskbar or ALT-TAB. Floating windows can have larger window rectangles
  // than what is visible to the user, so by filtering them out we will avoid
  // incorrectly marking native windows as occluded.
  if (ex_styles & WS_EX_TOOLWINDOW)
    return false;

  // Filter out layered windows.
  if (ex_styles & WS_EX_LAYERED)
    return false;

  // Filter out windows that do not have a simple rectangular region.
  base::win::ScopedRegion region(CreateRectRgn(0, 0, 0, 0));
  if (GetWindowRgn(hwnd, region.get()) == COMPLEXREGION)
    return false;

  RECT window_rect;
  // Filter out windows that take up zero area. The call to GetWindowRect is one
  // of the most expensive parts of this function, so it is last.
  if (!GetWindowRect(hwnd, &window_rect))
    return false;
  if (IsRectEmpty(&window_rect))
    return false;

  rect_in_pixels->SetByBounds(window_rect.left, window_rect.top,
                              window_rect.right, window_rect.bottom);
  return true;
}

}  // namespace aura_extra
