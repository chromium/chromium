// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/test_zwp_text_input_manager.h"

#include <wayland-server-core.h>

#include "ui/ozone/platform/wayland/test/mock_zwp_text_input.h"

namespace wl {

namespace {

constexpr uint32_t kTextInputManagerV1Version = 1;
constexpr uint32_t kTextInputManagerV3Version = 1;

void CreateTextInputV1(struct wl_client* client,
                       struct wl_resource* resource,
                       uint32_t id) {
  wl_resource* text_resource = CreateResourceWithImpl<MockZwpTextInputV1>(
      client, &zwp_text_input_v1_interface, wl_resource_get_version(resource),
      &kMockZwpTextInputV1Impl, id);
  GetUserDataAs<MockZwpTextInputV1>(text_resource)
      ->set_text_input_manager(
          GetUserDataAs<TestZwpTextInputManagerV1>(resource));
  GetUserDataAs<TestZwpTextInputManagerV1>(resource)->set_text_input(
      GetUserDataAs<MockZwpTextInputV1>(text_resource));
}

void CreateTextInputV3(struct wl_client* client,
                       struct wl_resource* resource,
                       uint32_t id,
                       struct wl_resource* seat) {
  wl_resource* text_resource = CreateResourceWithImpl<MockZwpTextInputV3>(
      client, &zwp_text_input_v3_interface, wl_resource_get_version(resource),
      &kMockZwpTextInputV3Impl, id);
  GetUserDataAs<MockZwpTextInputV3>(text_resource)
      ->set_text_input_manager(
          GetUserDataAs<TestZwpTextInputManagerV3>(resource));
  GetUserDataAs<TestZwpTextInputManagerV3>(resource)->set_text_input(
      GetUserDataAs<MockZwpTextInputV3>(text_resource));
}

}  // namespace

const struct zwp_text_input_manager_v1_interface
    kTestZwpTextInputManagerV1Impl = {
        &CreateTextInputV1,  // create_text_input
};

const struct zwp_text_input_manager_v3_interface
    kTestZwpTextInputManagerV3Impl = {
        &DestroyResource,
        &CreateTextInputV3,  // create_text_input
};

TestZwpTextInputManagerV1::TestZwpTextInputManagerV1()
    : GlobalObject(&zwp_text_input_manager_v1_interface,
                   &kTestZwpTextInputManagerV1Impl,
                   kTextInputManagerV1Version) {}

void TestZwpTextInputManagerV1::OnTextInputDestroyed(
    MockZwpTextInputV1* text_input) {
  text_input_ = nullptr;
}

TestZwpTextInputManagerV1::~TestZwpTextInputManagerV1() = default;

TestZwpTextInputManagerV3::TestZwpTextInputManagerV3()
    : GlobalObject(&zwp_text_input_manager_v3_interface,
                   &kTestZwpTextInputManagerV3Impl,
                   kTextInputManagerV3Version) {}

void TestZwpTextInputManagerV3::OnTextInputDestroyed(
    MockZwpTextInputV3* text_input) {
  text_input_ = nullptr;
}

TestZwpTextInputManagerV3::~TestZwpTextInputManagerV3() = default;

}  // namespace wl
