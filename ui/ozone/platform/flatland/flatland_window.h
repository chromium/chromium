// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_FLATLAND_FLATLAND_WINDOW_H_
#define UI_OZONE_PLATFORM_FLATLAND_FLATLAND_WINDOW_H_

#include <fuchsia/ui/composition/cpp/fidl.h>
#include <fuchsia/ui/input/cpp/fidl.h>
#include <fuchsia/ui/input3/cpp/fidl.h>
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
#include "ui/ozone/platform/flatland/flatland_connection.h"
#include "ui/platform_window/platform_window.h"
#include "ui/platform_window/platform_window_delegate.h"
#include "ui/platform_window/platform_window_init_properties.h"

namespace ui {

class FlatlandWindowManager;

class COMPONENT_EXPORT(OZONE) FlatlandWindow : public PlatformWindow,
                                               public InputEventSink {
 public:
  // Both |window_manager| and |delegate| must outlive the FlatlandWindow.
  //  GraphLinkToken is passed to Flatland to attach it to the scene graph.
  // |view_ref_pair| will be associated with this window's View, and used to
  // identify it when calling out to other services (e.g. the SemanticsManager).
  FlatlandWindow(FlatlandWindowManager* window_manager,
                 PlatformWindowDelegate* delegate,
                 PlatformWindowInitProperties properties);
  ~FlatlandWindow() override;
  FlatlandWindow(const FlatlandWindow&) = delete;
  FlatlandWindow& operator=(const FlatlandWindow&) = delete;

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
  // Callbacks from |graph_link_to_parent_|.
  void OnGetLayout(fuchsia::ui::composition::LayoutInfo info);

  // Called from link callbacks to handle view properties and metrics
  // changes.
  void OnViewProperties(const fuchsia::math::SizeU& properties);
  void OnViewMetrics(const fuchsia::math::SizeU& metrics);
  void OnViewAttachedChanged(bool is_view_attached);

  // Called to handle input events.
  void OnInputEvent(const fuchsia::ui::input::InputEvent& event);

  // InputEventSink implementation.
  void DispatchEvent(ui::Event* event) override;

  void UpdateSize();

  FlatlandWindowManager* const manager_;
  PlatformWindowDelegate* const delegate_;
  gfx::AcceleratedWidget const window_id_;

  // Dispatches Flatland input events as Chrome ui::Events.
  InputEventDispatcher event_dispatcher_;

  fuchsia::ui::input3::KeyboardPtr keyboard_service_;
  std::unique_ptr<KeyboardClient> keyboard_client_;

  // Handle to a kernel object which identifies this window's View
  // across the system. ViewRef consumers can access the handle by
  // calling CloneViewRef().
  fuchsia::ui::views::ViewRef view_ref_;

  // Flatland session used for all drawing operations in this View and safely
  // queueing Present() operations.
  FlatlandConnection flatland_;

  uint64_t next_transform_id_ = 1;
  fuchsia::ui::composition::TransformId root_transform_id_;
  fuchsia::ui::composition::TransformId render_transform_id_;
  fuchsia::ui::composition::TransformId surface_transform_id_;

  fuchsia::ui::composition::ContentId surface_content_id_;

  // TODO(crbug.com/1230150): Add link primitives.

  // The scale between logical pixels and physical pixels, set based on the
  // fuchsia::ui::gfx::Metrics event. It's used to calculate dimensions of the
  // view in physical pixels in UpdateSize(). This value doesn't affect the
  // device_scale_factor reported by FlatlandScreen for the corresponding
  // display (currently always 1.0, see crbug.com/1215330).
  float device_pixel_ratio_ = 0.f;

  // Current view size in DIPs.
  gfx::SizeF size_dips_;

  // Current view size in device pixels. The size is set to
  // |PlatformWindowInitProperties.bounds.size()| value until
  // |graph_link_to_parent_| is bound and returns OnGetLayout().
  gfx::Rect bounds_;

  absl::optional<fuchsia::math::SizeU> view_properties_;

  bool visible_ = false;
  bool virtual_keyboard_enabled_ = false;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_FLATLAND_FLATLAND_WINDOW_H_
