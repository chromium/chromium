// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_MOCK_XDG_ACTIVATION_V1_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_MOCK_XDG_ACTIVATION_V1_H_

#include <xdg-activation-v1-server-protocol.h>

#include "base/check.h"
#include "base/memory/raw_ptr.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/ozone/platform/wayland/test/global_object.h"
#include "ui/ozone/platform/wayland/test/server_object.h"

namespace wl {

extern const struct xdg_activation_token_v1_interface
    kMockXdgActivationTokenV1Impl;
extern const struct xdg_activation_v1_interface kMockXdgActivationV1Impl;

class MockXdgActivationTokenV1;

class MockXdgActivationV1 : public GlobalObject {
 public:
  MockXdgActivationV1();

  MockXdgActivationV1(const MockXdgActivationV1&) = delete;
  MockXdgActivationV1& operator=(const MockXdgActivationV1&) = delete;

  ~MockXdgActivationV1() override;

  MockXdgActivationTokenV1* get_token() { return token_; }
  void set_token(MockXdgActivationTokenV1* token) { token_ = token; }

  MOCK_METHOD4(Activate,
               void(struct wl_client* client,
                    struct wl_resource* resource,
                    const char* token,
                    struct wl_resource* surface));
  MOCK_METHOD3(TokenSetSurface,
               void(struct wl_client* client,
                    struct wl_resource* resource,
                    struct wl_resource* surface));

  MOCK_METHOD2(TokenCommit,
               void(struct wl_client* client, struct wl_resource* resource));

 private:
  raw_ptr<MockXdgActivationTokenV1, DanglingUntriaged> token_ = nullptr;
};

class MockXdgActivationTokenV1 : public ServerObject {
 public:
  explicit MockXdgActivationTokenV1(wl_resource* resource,
                                    MockXdgActivationV1* global);

  MockXdgActivationTokenV1(const MockXdgActivationTokenV1&) = delete;
  MockXdgActivationTokenV1& operator=(const MockXdgActivationTokenV1&) = delete;

  ~MockXdgActivationTokenV1() override;

  void SetSurface(struct wl_client* client,
                  struct wl_resource* resource,
                  struct wl_resource* surface) {
    global_->TokenSetSurface(client, resource, surface);
  }

  void Commit(struct wl_client* client, struct wl_resource* resource) {
    global_->TokenCommit(client, resource);
  }

 private:
  raw_ptr<MockXdgActivationV1> global_ = nullptr;
};

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_MOCK_XDG_ACTIVATION_V1_H_
