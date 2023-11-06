// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_ZWP_RELATIVE_POINTER_MANAGER_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_ZWP_RELATIVE_POINTER_MANAGER_H_

#include "base/memory/raw_ptr.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"

namespace base {
class TimeTicks;
}

namespace gfx {
class Vector2dF;
}  // namespace gfx

namespace ui {

class WaylandConnection;

// Wraps the zwp_relative_pointer_manager_v1 object.
class WaylandZwpRelativePointerManager
    : public wl::GlobalObjectRegistrar<WaylandZwpRelativePointerManager> {
 public:
  static constexpr char kInterfaceName[] = "zwp_relative_pointer_manager_v1";

  static void Instantiate(WaylandConnection* connection,
                          wl_registry* registry,
                          uint32_t name,
                          const std::string& interface,
                          uint32_t version);

  class Delegate;

  WaylandZwpRelativePointerManager(
      zwp_relative_pointer_manager_v1* relative_pointer_manager,
      WaylandConnection* connection);

  WaylandZwpRelativePointerManager(const WaylandZwpRelativePointerManager&) =
      delete;
  WaylandZwpRelativePointerManager& operator=(
      const WaylandZwpRelativePointerManager&) = delete;
  ~WaylandZwpRelativePointerManager();

  void EnableRelativePointer();
  void DisableRelativePointer();

 private:
  // zwp_relative_pointer_v1_listener callbacks:
  static void OnRelativeMotion(void* data,
                               zwp_relative_pointer_v1* pointer,
                               uint32_t utime_hi,
                               uint32_t utime_lo,
                               wl_fixed_t dx,
                               wl_fixed_t dy,
                               wl_fixed_t dx_unaccel,
                               wl_fixed_t dy_unaccel);

  wl::Object<zwp_relative_pointer_manager_v1> obj_;
  wl::Object<zwp_relative_pointer_v1> relative_pointer_;
  const raw_ptr<WaylandConnection> connection_;
  const raw_ptr<Delegate> delegate_;
};

class WaylandZwpRelativePointerManager::Delegate {
 public:
  virtual void SetRelativePointerMotionEnabled(bool enabled) = 0;
  virtual void OnRelativePointerMotion(const gfx::Vector2dF& delta,
                                       base::TimeTicks timestamp) = 0;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_ZWP_RELATIVE_POINTER_MANAGER_H_
