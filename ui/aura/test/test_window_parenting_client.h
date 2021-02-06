// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_TEST_TEST_WINDOW_PARENTING_CLIENT_H_
#define UI_AURA_TEST_TEST_WINDOW_PARENTING_CLIENT_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "ui/aura/client/window_parenting_client.h"

namespace aura {
namespace test {

class TestWindowParentingClient : public client::WindowParentingClient {
 public:
  explicit TestWindowParentingClient(Window* root_window);
  ~TestWindowParentingClient() override;

  void set_default_parent(Window* parent) { default_parent_ = parent; }

  // Overridden from client::WindowParentingClient:
  Window* GetDefaultParent(Window* window, const gfx::Rect& bounds) override;

 private:
  Window* root_window_;

  // If non-null this is returned from GetDefaultParent().
  Window* default_parent_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(TestWindowParentingClient);
};

}  // namespace test
}  // namespace aura

#endif  // UI_AURA_TEST_TEST_WINDOW_PARENTING_CLIENT_H_
