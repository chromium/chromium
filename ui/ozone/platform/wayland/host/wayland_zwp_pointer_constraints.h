// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_ZWP_POINTER_CONSTRAINTS_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_ZWP_POINTER_CONSTRAINTS_H_

#include "base/memory/raw_ptr.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"

namespace ui {

class WaylandConnection;
class WaylandSurface;

// Wraps the zwp_pointer_constraints_v1 object.
class WaylandZwpPointerConstraints
    : public wl::GlobalObjectRegistrar<WaylandZwpPointerConstraints> {
 public:
  static constexpr char kInterfaceName[] = "zwp_pointer_constraints_v1";

  static void Instantiate(WaylandConnection* connection,
                          wl_registry* registry,
                          uint32_t name,
                          const std::string& interface,
                          uint32_t version);

  WaylandZwpPointerConstraints(zwp_pointer_constraints_v1* pointer_constraints,
                               WaylandConnection* connection);

  WaylandZwpPointerConstraints(const WaylandZwpPointerConstraints&) = delete;
  WaylandZwpPointerConstraints& operator=(const WaylandZwpPointerConstraints&) =
      delete;
  ~WaylandZwpPointerConstraints();

  void LockPointer(WaylandSurface* surface);
  void UnlockPointer();

 private:
  // zwp_locked_pointer_v1_listener callbacks:
  static void OnLocked(void* data,
                       struct zwp_locked_pointer_v1* locked_pointer);
  static void OnUnlocked(void* data,
                         struct zwp_locked_pointer_v1* locked_pointer);

  wl::Object<zwp_pointer_constraints_v1> obj_;
  wl::Object<zwp_locked_pointer_v1> locked_pointer_;
  const raw_ptr<WaylandConnection> connection_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_ZWP_POINTER_CONSTRAINTS_H_
