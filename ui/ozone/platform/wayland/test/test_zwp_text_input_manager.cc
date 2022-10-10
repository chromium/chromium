// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/test_zwp_text_input_manager.h"

#include <wayland-server-core.h>

#include "ui/ozone/platform/wayland/test/mock_zwp_text_input.h"

namespace wl {

namespace {

constexpr uint32_t kTextInputManagerVersion = 1;

void CreateTextInput(struct wl_client* client,
                     struct wl_resource* resource,
                     uint32_t id) {
  wl_resource* text_resource = CreateResourceWithImpl<MockZwpTextInput>(
      client, &zwp_text_input_v1_interface, wl_resource_get_version(resource),
      &kMockZwpTextInputV1Impl, id);
  GetUserDataAs<TestZwpTextInputManagerV1>(resource)->set_text_input(
      GetUserDataAs<MockZwpTextInput>(text_resource));
}

}  // namespace

const struct zwp_text_input_manager_v1_interface
    kTestZwpTextInputManagerV1Impl = {
        &CreateTextInput,  // create_text_input
};

TestZwpTextInputManagerV1::TestZwpTextInputManagerV1()
    : GlobalObject(&zwp_text_input_manager_v1_interface,
                   &kTestZwpTextInputManagerV1Impl,
                   kTextInputManagerVersion) {}

TestZwpTextInputManagerV1::~TestZwpTextInputManagerV1() = default;

}  // namespace wl
