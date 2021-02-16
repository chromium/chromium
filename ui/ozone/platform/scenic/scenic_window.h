// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_SCENIC_SCENIC_WINDOW_H_
#define UI_OZONE_PLATFORM_SCENIC_SCENIC_WINDOW_H_

#include <fuchsia/ui/gfx/cpp/fidl.h>
#include <fuchsia/ui/input/cpp/fidl.h>
#include <lib/ui/scenic/cpp/resources.h>
#include <lib/ui/scenic/cpp/session.h>
#include <lib/ui/scenic/cpp/view_ref_pair.h>
#include <memory>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/macros.h"
#include "ui/events/fuchsia/input_event_dispatcher.h"
#include "ui/events/fuchsia/input_event_dispatcher_delegate.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/platform_window/platform_window.h"
#include "ui/platform_window/platform_window_delegate.h"
#include "ui/platform_window/platform_window_init_properties.h"

namespace ui {

class ScenicWindowManager;

class COMPONENT_EXPORT(OZONE) ScenicWindow
    : public PlatformWindow,
      public InputEventDispatcherDelegate {
 public:
  // Both |window_manager| and |delegate| must outlive the ScenicWindow.
  // |view_token| is passed to Scenic to attach the view to the view tree.
  // |view_ref_pair| will be associated with this window's View, and used to
  // identify it when calling out to other services (e.g. the SemanticsManager).
  ScenicWindow(ScenicWindowManager* window_manager,
               PlatformWindowDelegate* delegate,
               PlatformWindowInitProperties properties);
  ~ScenicWindow() override;

  scenic::Session* scenic_session() { return &scenic_session_; }

  // Embeds the View identified by |token| into the render node,
  // causing its contents to be displayed in this window.
  void AttachSurfaceView(fuchsia::ui::views::ViewHolderToken token);

  // Returns a ViewRef associated with this window.
  fuchsia::ui::views::ViewRef CloneViewRef();

  // PlatformWindow implementation.
  gfx::Rect GetBounds() const override;
  void SetBounds(const gfx::Rect& bounds) override;
  void SetTitle(const base::string16& title) override;
  void Show(bool inactive) override;
  void Hide() override;
  void Close() override;
  bool IsVisible() const override;
  void PrepareForShutdown() override;
  void SetCapture() override;
  void ReleaseCapture() override;
  bool HasCapture() const override;
  void ToggleFullscreen() override;
  void Maximize() override;
  void Minimize() override;
  void Restore() override;
  PlatformWindowState GetPlatformWindowState() const override;
  void Activate() override;
  void Deactivate() override;
  void SetUseNativeFrame(bool use_native_frame) override;
  bool ShouldUseNativeFrame() const override;
  void SetCursor(PlatformCursor cursor) override;
  void MoveCursorTo(const gfx::Point& location) override;
  void ConfineCursorToBounds(const gfx::Rect& bounds) override;
  void SetRestoredBoundsInPixels(const gfx::Rect& bounds) override;
  gfx::Rect GetRestoredBoundsInPixels() const override;
  void SetWindowIcons(const gfx::ImageSkia& window_icon,
                      const gfx::ImageSkia& app_icon) override;
  void SizeConstraintsChanged() override;

 private:
  // Callbacks for |scenic_session_|.
  void OnScenicError(zx_status_t status);
  void OnScenicEvents(std::vector<fuchsia::ui::scenic::Event> events);

  // Called from OnScenicEvents() to handle view properties and metrics changes.
  void OnViewProperties(const fuchsia::ui::gfx::ViewProperties& properties);
  void OnViewMetrics(const fuchsia::ui::gfx::Metrics& metrics);
  void OnViewAttachedChanged(bool is_view_attached);

  // Called from OnScenicEvents() to handle input events.
  void OnInputEvent(const fuchsia::ui::input::InputEvent& event);

  // InputEventDispatcher::Delegate interface.
  void DispatchEvent(ui::Event* event) override;

  void UpdateSize();

  ScenicWindowManager* const manager_;
  PlatformWindowDelegate* const delegate_;
  gfx::AcceleratedWidget const window_id_;

  // Dispatches Scenic input events as Chrome ui::Events.
  InputEventDispatcher event_dispatcher_;

  // Scenic session used for all drawing operations in this View.
  scenic::Session scenic_session_;

  // Handle to a kernel object which identifies this window's View
  // across the system. ViewRef consumers can access the handle by
  // calling CloneViewRef().
  fuchsia::ui::views::ViewRef view_ref_;

  // The view resource in |scenic_session_|.
  scenic::View view_;

  // Entity node for the |view_|.
  scenic::EntityNode node_;

  // Node in |scenic_session_| for receiving input that hits within our View.
  scenic::ShapeNode input_node_;

  // Node in |scenic_session_| for rendering (hit testing disabled).
  scenic::EntityNode render_node_;

  std::unique_ptr<scenic::ViewHolder> surface_view_holder_;

  // The ratio used for translating device-independent coordinates to absolute
  // pixel coordinates.
  float device_pixel_ratio_ = 0.f;

  // Current view size in DIPs.
  gfx::SizeF size_dips_;

  // Current view size in device pixels. The size is set to
  // |PlatformWindowInitProperties.bounds.size()| value until Show() is called
  // for the first time. After that the size is set to the size of the
  // corresponding Scenic view.
  gfx::Rect bounds_;

  bool visible_ = false;

  DISALLOW_COPY_AND_ASSIGN(ScenicWindow);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_SCENIC_SCENIC_WINDOW_H_
