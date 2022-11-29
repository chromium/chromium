// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_SCENIC_SCENIC_WINDOW_H_
#define UI_OZONE_PLATFORM_SCENIC_SCENIC_WINDOW_H_

#include <fuchsia/ui/gfx/cpp/fidl.h>
#include <fuchsia/ui/input/cpp/fidl.h>
#include <fuchsia/ui/input3/cpp/fidl.h>
#include <lib/ui/scenic/cpp/resources.h>
#include <lib/ui/scenic/cpp/session.h>
#include <lib/ui/scenic/cpp/view_ref_pair.h>
#include <memory>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "ui/base/ime/fuchsia/keyboard_client.h"
#include "ui/events/fuchsia/input_event_sink.h"
#include "ui/events/fuchsia/pointer_events_handler.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/platform/scenic/safe_presenter.h"
#include "ui/platform_window/platform_window.h"
#include "ui/platform_window/platform_window_delegate.h"
#include "ui/platform_window/platform_window_init_properties.h"

namespace ui {

class ScenicWindowDelegate;
class ScenicWindowManager;

class COMPONENT_EXPORT(OZONE) ScenicWindow final : public PlatformWindow,
                                                   public InputEventSink {
 public:
  // Both |window_manager| and |delegate| must outlive the ScenicWindow.
  // |view_token| is passed to Scenic to attach the view to the view tree.
  // |view_ref_pair| will be associated with this window's View, and used to
  // identify it when calling out to other services (e.g. the SemanticsManager).
  ScenicWindow(ScenicWindowManager* window_manager,
               PlatformWindowDelegate* delegate,
               PlatformWindowInitProperties properties);
  ~ScenicWindow() override;

  ScenicWindow(const ScenicWindow&) = delete;
  ScenicWindow& operator=(const ScenicWindow&) = delete;

  // Returns a ViewRef that may be used to refer to this window's View, when
  // interacting with View-based services such as the SemanticsManager.
  fuchsia::ui::views::ViewRef CloneViewRef();

  // ui::PlatformWindow implementation.
  gfx::Rect GetBoundsInPixels() const override;
  void SetBoundsInPixels(const gfx::Rect& bounds) override;
  gfx::Rect GetBoundsInDIP() const override;
  void SetBoundsInDIP(const gfx::Rect& bounds) override;
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

  // Used by ScenicGpuHost to embed the View identified by |token| into the
  // render node, causing its contents to be displayed in this window.
  void AttachSurfaceView(fuchsia::ui::views::ViewHolderToken token);

  // Used by OzoneScenicPlatform to determine whether to enable on-screen
  // keyboard features when creating the InputMethod for the window.
  bool is_virtual_keyboard_enabled() const {
    return is_virtual_keyboard_enabled_;
  }

 private:
  // ui::InputEventSink implementation, used to scale input event locations
  // according to the View's current device pixel ratio.
  void DispatchEvent(ui::Event* event) override;

  // Callbacks for |scenic_session_|.
  void OnScenicError(zx_status_t status);
  void OnScenicEvents(std::vector<fuchsia::ui::scenic::Event> events);

  // Called from OnScenicEvents() to handle view properties and metrics changes.
  void OnViewAttachedChanged(bool is_view_attached);
  void OnViewMetrics(const fuchsia::ui::gfx::Metrics& metrics);
  void OnViewProperties(const fuchsia::ui::gfx::ViewProperties& properties);

  // Called from OnScenicEvents() to handle focus change and input events.
  void OnInputEvent(const fuchsia::ui::input::InputEvent& event);

  // Hanging gets from |view_ref_focused_|.
  void OnViewRefFocusedWatchResult(fuchsia::ui::views::FocusState focus_state);

  // Sizes the Scenic nodes based on the View dimensions, and device pixel
  // ratio, and signals the dimensions change to the window delegate.
  void UpdateSize();

  // Attaches or detaches the root node to match the visibility and dimensions
  // of the window's View. Returns true if the root node is currently visible.
  bool UpdateRootNodeVisibility();

  // Returns true if the View has zero-sized dimensions set.
  bool is_zero_sized() const {
    return view_properties_ && ((view_properties_->bounding_box.max.x ==
                                 view_properties_->bounding_box.min.x) ||
                                (view_properties_->bounding_box.max.y ==
                                 view_properties_->bounding_box.min.y));
  }

  void OnViewControllerDisconnected(zx_status_t status);

  ScenicWindowManager* const manager_;
  PlatformWindowDelegate* const delegate_;
  ScenicWindowDelegate* const scenic_window_delegate_;
  gfx::AcceleratedWidget const window_id_;

  // Handle to a kernel object which identifies this window's View
  // across the system. ViewRef consumers can access the handle by
  // calling CloneViewRef().
  const fuchsia::ui::views::ViewRef view_ref_;

  // Used to coordinate window closure requests with the shell.
  fuchsia::element::ViewControllerPtr view_controller_;

  fuchsia::ui::input3::KeyboardPtr keyboard_service_;
  std::unique_ptr<KeyboardClient> keyboard_client_;

  // React to view-focus coming and going.
  fuchsia::ui::views::ViewRefFocusedPtr view_ref_focused_;

  // Accept touch and mouse events.
  absl::optional<PointerEventsHandler> pointer_handler_;

  // Scenic session used for all drawing operations in this View.
  absl::optional<scenic::Session> scenic_session_;

  // Used for safely queueing Present() operations on |scenic_session_|.
  absl::optional<SafePresenter> safe_presenter_;

  // The view resource in |scenic_session_|.
  absl::optional<scenic::View> view_;

  // Entity node for the |view_|.
  absl::optional<scenic::EntityNode> node_;

  // Node in |scenic_session_| for receiving input that hits within our View.
  absl::optional<scenic::ShapeNode> input_node_;

  // Node in |scenic_session_| for rendering (hit testing disabled).
  absl::optional<scenic::EntityNode> render_node_;

  // Holds the View into which the GPU processes composites the window's
  // contents.
  std::unique_ptr<scenic::ViewHolder> surface_view_holder_;

  // The scale between logical pixels and physical pixels, set based on the
  // fuchsia::ui::gfx::Metrics event. It's used to calculate dimensions of the
  // view in physical pixels in UpdateSize(). This value doesn't affect the
  // device_scale_factor reported by ScenicScreen for the corresponding display
  // (currently always 1.0, see crbug.com/1215330).
  float device_pixel_ratio_ = 0.f;

  // Current view size in DIPs.
  gfx::SizeF size_dips_;

  // Current view size in device pixels. The size is set to
  // |PlatformWindowInitProperties.bounds.size()| value until Show() is called
  // for the first time. After that the size is set to the size of the
  // corresponding Scenic view.
  gfx::Rect bounds_;

  // Holds the contents of the fuchsia.ui.gfx.Event.view_properties_changed
  // most recently received for the View, if any.
  absl::optional<fuchsia::ui::gfx::ViewProperties> view_properties_;

  // False if the View for this window is detached from the View tree, in which
  // case it is definitely not visible.
  bool is_visible_ = false;

  // True if the View occupies the full screen.
  bool is_fullscreen_ = false;

  // True if the on-screen virtual keyboard is available for this window.
  bool is_virtual_keyboard_enabled_ = false;

  // True if the root node is attached to the View, to be rendered.
  bool is_root_node_shown_ = false;

  // True if |view_| is currently attached to a scene.
  bool is_view_attached_ = false;

  // True if SetCapture() was called. Currently does not reflect capture state
  // in Scenic.
  // TODO(crbug.com/1231516): Use Scenic capture APIs.
  bool has_capture_ = false;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_SCENIC_SCENIC_WINDOW_H_
