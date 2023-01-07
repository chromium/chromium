// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/mock_buffer.h"

namespace wl {

const struct wl_buffer_interface kMockWlBufferImpl = {&DestroyResource};

MockBuffer::MockBuffer(wl_resource* resource, std::vector<base::ScopedFD>&& fds)
    : ServerObject(resource), fds_(std::move(fds)) {}

MockBuffer::~MockBuffer() {}

}  // namespace wl
