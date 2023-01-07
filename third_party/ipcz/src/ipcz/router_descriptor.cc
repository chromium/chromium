// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipcz/router_descriptor.h"

namespace ipcz {

RouterDescriptor::RouterDescriptor() = default;

RouterDescriptor::RouterDescriptor(const RouterDescriptor&) = default;

RouterDescriptor& RouterDescriptor::operator=(const RouterDescriptor&) =
    default;

RouterDescriptor::~RouterDescriptor() = default;

}  // namespace ipcz
