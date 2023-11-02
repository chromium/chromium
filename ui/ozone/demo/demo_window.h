// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_DEMO_DEMO_WINDOW_H_
#define UI_OZONE_DEMO_DEMO_WINDOW_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/platform_window/platform_window.h"
#include "ui/platform_window/platform_window_delegate.h"

namespace ui {

class Event;
class Renderer;
class RendererFactory;
class WindowManager;

class DemoWindow : public PlatformWindowDelegate {
 public:
  DemoWindow(WindowManager* window_manager,
             RendererFactory* renderer_factory,
             const gfx::Rect& bounds);

  DemoWindow(const DemoWindow&) = delete;
  DemoWindow& operator=(const DemoWindow&) = delete;

  ~DemoWindow() override;

  gfx::AcceleratedWidget GetAcceleratedWidget();

  gfx::Size GetSize();

  void Start();
  void Quit();

  // PlatformWindowDelegate:
  void OnBoundsChanged(const BoundsChange& change) override;
  void OnDamageRect(const gfx::Rect& damaged_region) override;
  void DispatchEvent(Event* event) override;
  void OnCloseRequest() override;
  void OnClosed() override;
  void OnWindowStateChanged(PlatformWindowState old_state,
                            PlatformWindowState new_state) override;
  void OnLostCapture() override;
  void OnAcceleratedWidgetAvailable(gfx::AcceleratedWidget widget) override;
  void OnWillDestroyAcceleratedWidget() override {}
  void OnAcceleratedWidgetDestroyed() override;
  void OnActivationChanged(bool active) override;
  void OnMouseEnter() override;

 private:
  // Since we pretend to have a GPU process, we should also pretend to
  // initialize the GPU resources via a posted task.
  void StartRendererIfNecessary();

  raw_ptr<WindowManager> window_manager_;      // Not owned.
  raw_ptr<RendererFactory> renderer_factory_;  // Not owned.

  std::unique_ptr<Renderer> renderer_;

  // Window-related state.
  std::unique_ptr<PlatformWindow> platform_window_;
  gfx::AcceleratedWidget widget_ = gfx::kNullAcceleratedWidget;

  base::WeakPtrFactory<DemoWindow> weak_ptr_factory_{this};
};

}  // namespace ui

#endif  // UI_OZONE_DEMO_DEMO_WINDOW_H_
