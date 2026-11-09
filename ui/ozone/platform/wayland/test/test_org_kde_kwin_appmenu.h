// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_ORG_KDE_KWIN_APPMENU_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_ORG_KDE_KWIN_APPMENU_H_

#include "ui/ozone/platform/wayland/test/global_object.h"

namespace wl {

// Manages the org_kde_kwin_appmenu_manager global, which is exported by KWin.
class TestOrgKdeKwinAppmenuManager : public GlobalObject {
 public:
  TestOrgKdeKwinAppmenuManager();
  TestOrgKdeKwinAppmenuManager(const TestOrgKdeKwinAppmenuManager&) = delete;
  TestOrgKdeKwinAppmenuManager& operator=(const TestOrgKdeKwinAppmenuManager&) =
      delete;
  ~TestOrgKdeKwinAppmenuManager() override;
};

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_ORG_KDE_KWIN_APPMENU_H_
