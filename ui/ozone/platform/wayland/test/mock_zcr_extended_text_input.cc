// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/mock_zcr_extended_text_input.h"

namespace wl {

const struct zcr_extended_text_input_v1_interface
    kMockZcrExtendedTextInputV1Impl = {
        &DestroyResource,  // destroy
};

MockZcrExtendedTextInput::MockZcrExtendedTextInput(wl_resource* resource)
    : ServerObject(resource) {}

MockZcrExtendedTextInput::~MockZcrExtendedTextInput() = default;

}  // namespace wl
