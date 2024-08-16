// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_MOCK_XDG_TOPLEVEL_ICON_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_MOCK_XDG_TOPLEVEL_ICON_H_

#include <xdg-toplevel-icon-v1-server-protocol.h>

#include <vector>

#include "base/check.h"
#include "base/memory/raw_ptr.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/gfx/geometry/size.h"
#include "ui/ozone/platform/wayland/test/global_object.h"
#include "ui/ozone/platform/wayland/test/server_object.h"

namespace wl {

class MockXdgToplevelIconV1;

struct XdgToplevelIconResource {
  gfx::Size size;
  int32_t scale;

  bool operator==(const XdgToplevelIconResource& other) const {
    return size == other.size && scale == other.scale;
  }
};

using XdgToplevelIconResources = std::vector<XdgToplevelIconResource>;

extern const struct xdg_toplevel_icon_manager_v1_interface
    kMockXdgToplevelIconManagerImpl;
extern const struct xdg_toplevel_icon_v1_interface kMockXdgToplevelIconImpl;

class MockXdgToplevelIconManagerV1 : public GlobalObject {
 public:
  MockXdgToplevelIconManagerV1();

  MockXdgToplevelIconManagerV1(const MockXdgToplevelIconManagerV1&) = delete;
  MockXdgToplevelIconManagerV1& operator=(const MockXdgToplevelIconManagerV1&) =
      delete;

  ~MockXdgToplevelIconManagerV1() override;

  void set_icon(MockXdgToplevelIconV1* icon) { icon_ = icon; }
  MockXdgToplevelIconV1* icon() { return icon_; }

  XdgToplevelIconResources& resources() { return resources_; }

 private:
  raw_ptr<MockXdgToplevelIconV1> icon_ = nullptr;

  XdgToplevelIconResources resources_;
};

class MockXdgToplevelIconV1 : public ServerObject {
 public:
  MockXdgToplevelIconV1(wl_resource* resource,
                        MockXdgToplevelIconManagerV1* global);

  MockXdgToplevelIconV1(const MockXdgToplevelIconV1&) = delete;
  MockXdgToplevelIconV1& operator=(const MockXdgToplevelIconV1&) = delete;

  ~MockXdgToplevelIconV1() override;

  XdgToplevelIconResources& resources() { return resources_; }

 private:
  raw_ptr<MockXdgToplevelIconManagerV1> global_ = nullptr;

  XdgToplevelIconResources resources_;
};

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_MOCK_XDG_TOPLEVEL_ICON_H_
