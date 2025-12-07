// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_ORG_KDE_KWIN_APPMENU_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_ORG_KDE_KWIN_APPMENU_H_

#include "base/memory/raw_ptr.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"

namespace ui {

class OrgKdeKwinAppmenu {
 public:
  explicit OrgKdeKwinAppmenu(org_kde_kwin_appmenu* appmenu);
  OrgKdeKwinAppmenu(const OrgKdeKwinAppmenu&) = delete;
  OrgKdeKwinAppmenu& operator=(const OrgKdeKwinAppmenu&) = delete;
  ~OrgKdeKwinAppmenu();

  void SetAddress(const std::string& service_name,
                  const std::string& object_path);

 private:
  wl::Object<org_kde_kwin_appmenu> appmenu_;
};

// Wraps the KDE Wayland appmenu manager, which is provided via
// org_kde_kwin_appmenu_manager interface.
class OrgKdeKwinAppmenuManager
    : public wl::GlobalObjectRegistrar<OrgKdeKwinAppmenuManager> {
 public:
  static constexpr const char kInterfaceName[] = "org_kde_kwin_appmenu_manager";

  static void Instantiate(WaylandConnection* connection,
                          wl_registry* registry,
                          uint32_t name,
                          const std::string& interface,
                          uint32_t version);

  OrgKdeKwinAppmenuManager(org_kde_kwin_appmenu_manager* manager,
                           WaylandConnection* connection);
  OrgKdeKwinAppmenuManager(const OrgKdeKwinAppmenuManager&) = delete;
  OrgKdeKwinAppmenuManager& operator=(const OrgKdeKwinAppmenuManager&) = delete;
  ~OrgKdeKwinAppmenuManager();

  std::unique_ptr<OrgKdeKwinAppmenu> Create(wl_surface* surface);

 private:
  wl::Object<org_kde_kwin_appmenu_manager> manager_;

  const raw_ptr<WaylandConnection> connection_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_ORG_KDE_KWIN_APPMENU_H_
