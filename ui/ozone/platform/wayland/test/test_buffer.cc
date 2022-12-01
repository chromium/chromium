// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/test_buffer.h"

namespace wl {

const struct wl_buffer_interface kTestWlBufferImpl = {&DestroyResource};

TestBuffer::TestBuffer(wl_resource* resource, std::vector<base::ScopedFD>&& fds)
    : ServerObject(resource), fds_(std::move(fds)) {}

TestBuffer::~TestBuffer() = default;

}  // namespace wl
