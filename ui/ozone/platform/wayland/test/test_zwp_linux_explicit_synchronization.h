// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_ZWP_LINUX_EXPLICIT_SYNCHRONIZATION_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_ZWP_LINUX_EXPLICIT_SYNCHRONIZATION_H_

#include "base/memory/raw_ptr.h"
#include "ui/ozone/platform/wayland/test/global_object.h"

#include "ui/ozone/platform/wayland/test/server_object.h"

namespace wl {

class TestLinuxSurfaceSynchronization : public ServerObject {
 public:
  TestLinuxSurfaceSynchronization(wl_resource* resource,
                                  wl_resource* surface_resource);
  ~TestLinuxSurfaceSynchronization() override;

  wl_resource* surface_resource() const { return surface_resource_; }

 private:
  raw_ptr<wl_resource, DanglingUntriaged> surface_resource_;
};

// Manage wl_viewporter object.
class TestZwpLinuxExplicitSynchronizationV1 : public GlobalObject {
 public:
  TestZwpLinuxExplicitSynchronizationV1();
  ~TestZwpLinuxExplicitSynchronizationV1() override;
  TestZwpLinuxExplicitSynchronizationV1(
      const TestZwpLinuxExplicitSynchronizationV1& rhs) = delete;
  TestZwpLinuxExplicitSynchronizationV1& operator=(
      const TestZwpLinuxExplicitSynchronizationV1& rhs) = delete;
};

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_ZWP_LINUX_EXPLICIT_SYNCHRONIZATION_H_
