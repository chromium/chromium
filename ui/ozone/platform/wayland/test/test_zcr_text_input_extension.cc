// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/test_zcr_text_input_extension.h"

#include <wayland-server-core.h>

#include "ui/ozone/platform/wayland/test/mock_zcr_extended_text_input.h"

namespace wl {

namespace {

void GetExtendedTextInput(struct wl_client* client,
                          struct wl_resource* resource,
                          uint32_t id,
                          struct wl_resource* text_input_resource) {
  wl_resource* text_resource = CreateResourceWithImpl<MockZcrExtendedTextInput>(
      client, &zcr_extended_text_input_v1_interface,
      wl_resource_get_version(resource), &kMockZcrExtendedTextInputV1Impl, id);
  GetUserDataAs<TestZcrTextInputExtensionV1>(resource)->set_extended_text_input(
      GetUserDataAs<MockZcrExtendedTextInput>(text_resource));
}

}  // namespace

const struct zcr_text_input_extension_v1_interface
    kTestZcrTextInputExtensionV1Impl = {
        &GetExtendedTextInput,  // get_extended_text_input
};

TestZcrTextInputExtensionV1::TestZcrTextInputExtensionV1(
    TestZcrTextInputExtensionV1::Version version)
    : GlobalObject(&zcr_text_input_extension_v1_interface,
                   &kTestZcrTextInputExtensionV1Impl,
                   static_cast<uint32_t>(version)) {}

TestZcrTextInputExtensionV1::~TestZcrTextInputExtensionV1() = default;

}  // namespace wl
