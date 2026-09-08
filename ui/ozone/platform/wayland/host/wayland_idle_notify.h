// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_IDLE_NOTIFY_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_IDLE_NOTIFY_H_

#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"

namespace ui {

class WaylandConnection;

// TODO(crbug.com/380125108) Add unit tests
// Wraps the ext_idle_notifier_v1 Wayland protocol, which provides user idle
// time notifications.
class ExtIdleNotifier : public wl::GlobalObjectRegistrar<ExtIdleNotifier> {
 public:
  static constexpr char kInterfaceName[] = "ext_idle_notifier_v1";

  static void Instantiate(WaylandConnection* connection,
                          wl_registry* registry,
                          uint32_t name,
                          const std::string& interface,
                          uint32_t version);

  ExtIdleNotifier(ext_idle_notifier_v1* idle, WaylandConnection* connection);
  ExtIdleNotifier(const ExtIdleNotifier&) = delete;
  ExtIdleNotifier& operator=(const ExtIdleNotifier&) = delete;
  ~ExtIdleNotifier();

  // Returns the idle time if querying it is possible, std::nullopt otherwise.
  std::optional<base::TimeDelta> GetIdleTime() const;

 private:
  class Timeout;

  // Wayland object wrapped by this class.
  wl::Object<ext_idle_notifier_v1> idle_;
  // The actual idle notification object.
  mutable std::unique_ptr<Timeout> idle_timeout_;

  const raw_ptr<WaylandConnection> connection_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_IDLE_NOTIFY_H_
