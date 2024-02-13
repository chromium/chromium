// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_FLATLAND_FLATLAND_WINDOW_H_
#define UI_OZONE_PLATFORM_FLATLAND_FLATLAND_WINDOW_H_

#include <fidl/fuchsia.ui.input3/cpp/fidl.h>
#include <fuchsia/element/cpp/fidl.h>
#include <fuchsia/ui/composition/cpp/fidl.h>
#include <fuchsia/ui/views/cpp/fidl.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/fuchsia/fidl_event_handler.h"
#include "base/functional/callback.h"
#include "ui/base/ime/fuchsia/keyboard_client.h"
#include "ui/events/fuchsia/input_event_sink.h"
#include "ui/events/fuchsia/pointer_events_handler.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/platform/flatland/flatland_connection.h"
#include "ui/platform_window/platform_window.h"
#include "ui/platform_window/platform_window_delegate.h"
#include "ui/platform_window/platform_window_init_properties.h"

namespace ui {

class FlatlandWindowManager;
class ScenicWindowDelegate;

class COMPONENT_EXPORT(OZONE) FlatlandWindow : public PlatformWindow,
                                               public InputEventSink {
 public:
  // Both |window_manager| and |delegate| must outlive the FlatlandWindow.
  // ViewCreationToken is passed to Flatland to attach it to the scene graph.
  // ViewrefPair will be associated with this window's View, and used to
  // identify it when calling out to other services (e.g. the SemanticsManager).
  FlatlandWindow(FlatlandWindowManager* window_manager,
                 PlatformWindowDelegate* platform_window_delegate,
                 PlatformWindowInitProperties properties);
  ~FlatlandWindow() override;
  FlatlandWindow(const FlatlandWindow&) = delete;
  FlatlandWindow& operator=(const FlatlandWindow&) = delete;

  void ResetSurfaceContent();

  // Embeds the Flatland identified by |token| into the scene graph.
  void AttachSurfaceContent(fuchsia::ui::views::ViewportCreationToken token);

  // Returns a ViewRef associated with this window.
  fuchsia::ui::views::ViewRef CloneViewRef();

  // Used by OzonePlatformFlatland to determine whether to enable on-screen
  // keyboard features when creating the InputMethod for the window.
  bool virtual_keyboard_enabled() const { return is_virtual_keyboard_enabled_; }

  // Test only PlatformWindow implementation.
  gfx::Rect GetBoundsInPixels() const override;
  void SetBoundsInPixels(const gfx::Rect& bounds) override;
  gfx::Rect GetBoundsInDIP() const override;
  void SetBoundsInDIP(const gfx::Rect& bounds) override;

  // PlatformWindow implementation.
  void SetTitle(const std::u16string& title) override;
  void Show(bool inactive) override;
  void Hide() override;
  void Close() override;
  bool IsVisible() const override;
  void PrepareForShutdown() override;
  void SetCapture() override;
  void ReleaseCapture() override;
  bool HasCapture() const override;
  void SetFullscreen(bool fullscreen, int64_t target_display_id) override;
  void Maximize() override;
  void Minimize() override;
  void Restore() override;
  PlatformWindowState GetPlatformWindowState() const override;
  void Activate() override;
  void Deactivate() override;
  void SetUseNativeFrame(bool use_native_frame) override;
  bool ShouldUseNativeFrame() const override;
  void SetCursor(scoped_refptr<PlatformCursor> cursor) override;
  void MoveCursorTo(const gfx::Point& location) override;
  void ConfineCursorToBounds(const gfx::Rect& bounds) override;
  void SetRestoredBoundsInDIP(const gfx::Rect& bounds) override;
  gfx::Rect GetRestoredBoundsInDIP() const override;
  void SetWindowIcons(const gfx::ImageSkia& window_icon,
                      const gfx::ImageSkia& app_icon) override;
  void SizeConstraintsChanged() override;

 private:
  friend class FlatlandWindowTest;

  // Hanging gets from |parent_viewport_watcher_|.
  void OnGetLayout(fuchsia::ui::composition::LayoutInfo info);
  void OnGetStatus(fuchsia::ui::composition::ParentViewportStatus status);

  // Hanging gets from |view_ref_focused_|.
  void OnViewRefFocusedWatchResult(fuchsia::ui::views::FocusState focus_state);

  // Called from link callbacks to handle view properties and metrics
  // changes.
  void OnViewAttachedChanged(bool is_view_attached);

  // InputEventSink implementation.
  void DispatchEvent(ui::Event* event) override;

  void UpdateSize();

  void OnFlatlandError(fuchsia::ui::composition::FlatlandError error);
  void OnViewControllerDisconnected(zx_status_t status);

  FlatlandWindowManager* const manager_;
  PlatformWindowDelegate* const platform_window_delegate_;
  ScenicWindowDelegate* const scenic_window_delegate_;
  gfx::AcceleratedWidget const window_id_;

  fidl::Client<fuchsia_ui_input3::Keyboard> keyboard_fidl_client_;
  base::FidlErrorEventLogger<fuchsia_ui_input3::Keyboard>
      fidl_error_event_logger_;
  std::unique_ptr<KeyboardClient> keyboard_client_;
  std::unique_ptr<PointerEventsHandler> pointer_handler_;

  // Handle to a kernel object which identifies this window's View
  // across the system. ViewRef consumers can access the handle by
  // calling CloneViewRef().
  fuchsia::ui::views::ViewRef view_ref_;

  // Used to coordinate window closure requests with the shell.
  fuchsia::element::ViewControllerPtr view_controller_;

  // Flatland session used for all drawing operations in this View and safely
  // queueing Present() operations.
  FlatlandConnection flatland_;

  fuchsia::ui::composition::TransformId root_transform_id_;
  fuchsia::ui::composition::TransformId surface_transform_id_;
  fuchsia::ui::composition::TransformId shield_transform_id_;

  fuchsia::ui::composition::ContentId surface_content_id_;

  // Pending Viewport creation callback waiting on |logical_size_|.
  base::OnceClosure pending_attach_surface_content_closure_;

  fuchsia::ui::composition::ParentViewportWatcherPtr parent_viewport_watcher_;

  // Protocol for watching focus changes.
  fuchsia::ui::views::ViewRefFocusedPtr view_ref_focused_;

  // Flatland View size in logical pixels.
  std::optional<gfx::Size> logical_size_;

  // The scale between logical pixels and physical pixels, set based on the
  // fuchsia::ui::composition::LayoutInfo. It's used to calculate dimensions of
  // the view in physical pixels in UpdateSize(). This value doesn't affect the
  // device_scale_factor reported by FlatlandScreen for the corresponding
  // display (currently always 1.0, see crbug.com/1215330).
  float device_pixel_ratio_ = 1.f;

  // Current view size in DIPs.
  gfx::SizeF size_dips_;

  // Current view size in device pixels. The size is set to
  // |PlatformWindowInitProperties.bounds.size()| value until
  // |parent_viewport_watcher_| is bound and returns OnGetLayout().
  gfx::Rect bounds_;

  // The offsets between the edges and the visible rectangle of the View, set
  // based on fuchsia::ui::composition::LayoutInfo. Used to set
  // `bounds_.system_ui_overlap` in `UpdateSize()`.
  gfx::Insets view_inset_;

  // False if the View for this window is detached from the View tree, in which
  // case it is definitely not visible.
  bool is_visible_ = false;

  // True if the View occupies the full screen.
  bool is_fullscreen_ = false;

  // True if the on-screen virtual keyboard is available for this window.
  bool is_virtual_keyboard_enabled_ = false;

  // True if |view_| is currently attached to a scene.
  bool is_view_attached_ = false;

  // True if SetCapture() was called. Currently does not reflect capture state
  // in Scenic.
  bool has_capture_ = false;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_FLATLAND_FLATLAND_WINDOW_H_
