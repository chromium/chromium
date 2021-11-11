// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_MOCK_BUFFER_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_MOCK_BUFFER_H_

#include <linux-dmabuf-unstable-v1-server-protocol.h>

#include "base/files/scoped_file.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/ozone/platform/wayland/test/server_object.h"

struct wl_resource;

namespace wl {

extern const struct wl_buffer_interface kMockWlBufferImpl;

// Manage wl_buffer object.
class MockBuffer : public ServerObject {
 public:
  MockBuffer(wl_resource* resource, std::vector<base::ScopedFD>&& fds);

  MockBuffer(const MockBuffer&) = delete;
  MockBuffer& operator=(const MockBuffer&) = delete;

  ~MockBuffer() override;

 private:
  std::vector<base::ScopedFD> fds_;
};

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_MOCK_BUFFER_H_
