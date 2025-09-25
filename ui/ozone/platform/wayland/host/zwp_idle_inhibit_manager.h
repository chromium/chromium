// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_ZWP_IDLE_INHIBIT_MANAGER_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_ZWP_IDLE_INHIBIT_MANAGER_H_

#include "base/memory/weak_ptr.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"

namespace ui {

class WaylandConnection;
class WaylandWindow;

// Wraps the idle inhibit manager, which is provided via
// zwp_idle_inhibit_manager_v1 interface.
class ZwpIdleInhibitManager
    : public wl::GlobalObjectRegistrar<ZwpIdleInhibitManager> {
 public:
  static constexpr char kInterfaceName[] = "zwp_idle_inhibit_manager_v1";

  static void Instantiate(WaylandConnection* connection,
                          wl_registry* registry,
                          uint32_t name,
                          const std::string& interface,
                          uint32_t version);

  explicit ZwpIdleInhibitManager(zwp_idle_inhibit_manager_v1* manager,
                                 WaylandConnection* connection);
  ZwpIdleInhibitManager(const ZwpIdleInhibitManager&) = delete;
  ZwpIdleInhibitManager& operator=(const ZwpIdleInhibitManager&) = delete;
  ~ZwpIdleInhibitManager();

  bool CreateInhibitor();
  void RemoveInhibitor();

  bool is_inhibiting() const { return !!inhibiting_window_; }

 private:
  const raw_ptr<WaylandConnection> connection_;

  // Wayland object wrapped by this class.
  wl::Object<zwp_idle_inhibit_manager_v1> manager_;

  base::WeakPtr<WaylandWindow> inhibiting_window_;
  wl::Object<zwp_idle_inhibitor_v1> idle_inhibitor_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_ZWP_IDLE_INHIBIT_MANAGER_H_
