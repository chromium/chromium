// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_PLATFORM_WINDOW_PLATFORM_WINDOW_INIT_PROPERTIES_H_
#define UI_PLATFORM_WINDOW_PLATFORM_WINDOW_INIT_PROPERTIES_H_

#include <optional>
#include <string>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"

#if BUILDFLAG(IS_FUCHSIA)
#include <fuchsia/element/cpp/fidl.h>
#include <fuchsia/ui/composition/cpp/fidl.h>
#include <fuchsia/ui/views/cpp/fidl.h>
#include <ui/platform_window/fuchsia/view_ref_pair.h>
#endif

namespace gfx {
class ImageSkia;
}

namespace ui {

enum class PlatformWindowType {
  kWindow,
  kPopup,
  kMenu,
  kTooltip,
  kDrag,
  kBubble,
};

enum class PlatformWindowOpacity {
  kInferOpacity,
  kOpaqueWindow,
  kTranslucentWindow,
};

enum class PlatformWindowShadowType {
  kDefault,
  kNone,
  kDrop,
};

class WorkspaceExtensionDelegate;

#if BUILDFLAG(IS_FUCHSIA)
class ScenicWindowDelegate;
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
class X11ExtensionDelegate;
#endif

// Initial properties which are passed to PlatformWindow to be initialized
// with a desired set of properties.
struct COMPONENT_EXPORT(PLATFORM_WINDOW) PlatformWindowInitProperties {
  PlatformWindowInitProperties();

  // Initializes properties with the specified |bounds|.
  explicit PlatformWindowInitProperties(const gfx::Rect& bounds);

  PlatformWindowInitProperties(PlatformWindowInitProperties&& props);
  PlatformWindowInitProperties& operator=(PlatformWindowInitProperties&&);

  ~PlatformWindowInitProperties();

  // Tells desired PlatformWindow type. It can be popup, menu or anything else.
  PlatformWindowType type = PlatformWindowType::kWindow;
  // Sets the desired initial bounds. Can be empty.
  gfx::Rect bounds;
  // Tells PlatformWindow which native widget its parent holds. It is usually
  // used to find a parent from internal list of PlatformWindows.
  gfx::AcceleratedWidget parent_widget = gfx::kNullAcceleratedWidget;
  // Tells the opacity type of a window. Check the comment in the
  // Widget::InitProperties::WindowOpacity.
  PlatformWindowOpacity opacity = PlatformWindowOpacity::kOpaqueWindow;

#if BUILDFLAG(IS_FUCHSIA)
  // Scenic 3D API uses `view_token` for links, whereas Flatland
  // API uses `view_creation_token`. Therefore, at most one of these fields must
  // be set. If `allow_null_view_token_for_test` is true, they may both be
  // false.
  fuchsia::ui::views::ViewToken view_token;
  fuchsia::ui::views::ViewCreationToken view_creation_token;

  ViewRefPair view_ref_pair;

  // Used to coordinate window closure requests with the shell.
  fuchsia::element::ViewControllerPtr view_controller;

  // Specifies whether handling of keypress events from the system is enabled.
  bool enable_keyboard = false;

  // Specifies whether system virtual keyboard support is enabled.
  bool enable_virtual_keyboard = false;

  ScenicWindowDelegate* scenic_window_delegate = nullptr;
#endif

  // See Widget::InitParams for details.
  bool accept_events = true;
  bool activatable = true;
  bool force_show_in_taskbar;
  bool keep_on_top = false;
  bool is_security_surface = false;
  bool visible_on_all_workspaces = false;
  bool remove_standard_frame = false;
  std::string workspace;
  ZOrderLevel z_order = ZOrderLevel::kNormal;

  raw_ptr<WorkspaceExtensionDelegate> workspace_extension_delegate = nullptr;

  PlatformWindowShadowType shadow_type = PlatformWindowShadowType::kDefault;

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  bool prefer_dark_theme = false;
  raw_ptr<gfx::ImageSkia> icon = nullptr;
  std::optional<SkColor> background_color;

  // Specifies the res_name and res_class fields,
  // respectively, of the WM_CLASS window property. Controls window grouping
  // and desktop file matching in Linux window managers.
  std::string wm_role_name;
  std::string wm_class_name;
  std::string wm_class_class;

  raw_ptr<X11ExtensionDelegate> x11_extension_delegate = nullptr;

  // Wayland specific.  Holds the application ID that is used by the window
  // manager to match the desktop entry and group windows.
  std::string wayland_app_id;

  // Specifies the unique session id and the restore window id.
  int32_t restore_session_id;
  std::optional<int32_t> restore_window_id;

  // Specifies the source to get `restore_window_id` from.
  std::optional<std::string> restore_window_id_source;

  // Specifies whether the associated window is persistable.
  bool persistable = true;

  // Specifies the id of the target display the window will be created on.
  std::optional<int64_t> display_id;
#endif

#if BUILDFLAG(IS_OZONE)
  // Specifies whether the current window requests key-events that matches
  // system shortcuts.
  bool inhibit_keyboard_shortcuts = false;
#endif

  bool enable_compositing_based_throttling = false;

  size_t compositor_memory_limit_mb = 0;
};

}  // namespace ui

#endif  // UI_PLATFORM_WINDOW_PLATFORM_WINDOW_INIT_PROPERTIES_H_
