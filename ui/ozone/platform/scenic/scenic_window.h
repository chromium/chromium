// Copyright 2018 The Chromium Authors. All rights reserved.
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
#include "base/macros.h"
#include "ui/base/ime/fuchsia/keyboard_client.h"
#include "ui/events/fuchsia/input_event_dispatcher.h"
#include "ui/events/fuchsia/input_event_sink.h"
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

class COMPONENT_EXPORT(OZONE) ScenicWindow : public PlatformWindow,
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

  // Converts Scenic's rect-based representation of insets to gfx::Insets.
  // Returns zero-width insets if |inset_from_min| and |inset_from_max| are
  // uninitialized (indicating that no insets were provided from Scenic).
  static gfx::Insets ConvertInsets(
      float device_pixel_ratio,
      const fuchsia::ui::gfx::ViewProperties& view_properties);

  scenic::Session* scenic_session() { return &scenic_session_; }

  // Embeds the View identified by |token| into the render node,
  // causing its contents to be displayed in this window.
  void AttachSurfaceView(fuchsia::ui::views::ViewHolderToken token);

  // Returns a ViewRef associated with this window.
  fuchsia::ui::views::ViewRef CloneViewRef();

  bool virtual_keyboard_enabled() const { return virtual_keyboard_enabled_; }

  // PlatformWindow implementation.
  gfx::Rect GetBounds() const override;
  void SetBounds(const gfx::Rect& bounds) override;
  void SetTitle(const std::u16string& title) override;
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
  void SetCursor(scoped_refptr<PlatformCursor> cursor) override;
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

  // InputEventSink implementation.
  void DispatchEvent(ui::Event* event) override;

  void UpdateSize();

  ScenicWindowManager* const manager_;
  PlatformWindowDelegate* const delegate_;
  ScenicWindowDelegate* const scenic_window_delegate_;
  gfx::AcceleratedWidget const window_id_;

  // Dispatches Scenic input events as Chrome ui::Events.
  InputEventDispatcher event_dispatcher_;

  fuchsia::ui::input3::KeyboardPtr keyboard_service_;
  std::unique_ptr<KeyboardClient> keyboard_client_;

  // Scenic session used for all drawing operations in this View.
  scenic::Session scenic_session_;

  // Used for safely queueing Present() operations on |scenic_session_|.
  SafePresenter safe_presenter_;

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

  absl::optional<fuchsia::ui::gfx::ViewProperties> view_properties_;

  bool visible_ = false;
  bool virtual_keyboard_enabled_ = false;

  // Tracks if the View was previously hidden due to having a size of zero.
  // If the View was previously zero sized, then we need to re-attach it to
  // its parent before we change its size to non-zero; and vice versa.
  bool previous_view_is_zero_sized_ = false;

  DISALLOW_COPY_AND_ASSIGN(ScenicWindow);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_SCENIC_SCENIC_WINDOW_H_
