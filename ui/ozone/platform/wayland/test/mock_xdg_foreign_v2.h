// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_MOCK_XDG_FOREIGN_V2_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_MOCK_XDG_FOREIGN_V2_H_

#include <xdg-foreign-unstable-v2-server-protocol.h>

#include <string>
#include <utility>

#include "testing/gmock/include/gmock/gmock.h"
#include "ui/ozone/platform/wayland/test/global_object.h"

struct wl_resource;

namespace wl {

extern const struct zxdg_exported_v2_interface kMockXdgExportedV2Impl;
extern const struct zxdg_exporter_v2_interface kMockXdgExporterV2Impl;

// Mock zxdg_exporter_v2 global. Exported surfaces are reported to
// ExportToplevel() and receive the handle set by set_handle().
class MockXdgExporterV2 : public GlobalObject {
 public:
  MockXdgExporterV2();

  MockXdgExporterV2(const MockXdgExporterV2&) = delete;
  MockXdgExporterV2& operator=(const MockXdgExporterV2&) = delete;

  ~MockXdgExporterV2() override;

  MOCK_METHOD(void, ExportToplevel, (wl_resource * surface));

  void set_handle(std::string handle) { handle_ = std::move(handle); }
  const std::string& handle() const { return handle_; }

 private:
  std::string handle_ = "test-xdg-foreign-handle";
};

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_MOCK_XDG_FOREIGN_V2_H_
