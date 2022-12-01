// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_BUFFER_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_BUFFER_H_

#include <linux-dmabuf-unstable-v1-server-protocol.h>

#include "base/files/scoped_file.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/ozone/platform/wayland/test/server_object.h"

struct wl_resource;

namespace wl {

extern const struct wl_buffer_interface kTestWlBufferImpl;

// Manage wl_buffer object.
class TestBuffer : public ServerObject {
 public:
  TestBuffer(wl_resource* resource, std::vector<base::ScopedFD>&& fds);

  TestBuffer(const TestBuffer&) = delete;
  TestBuffer& operator=(const TestBuffer&) = delete;

  ~TestBuffer() override;

 private:
  std::vector<base::ScopedFD> fds_;
};

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_BUFFER_H_
