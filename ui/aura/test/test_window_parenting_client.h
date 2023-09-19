// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_TEST_TEST_WINDOW_PARENTING_CLIENT_H_
#define UI_AURA_TEST_TEST_WINDOW_PARENTING_CLIENT_H_

#include "base/memory/raw_ptr.h"
#include "ui/aura/client/window_parenting_client.h"

namespace aura {
namespace test {

class TestWindowParentingClient : public client::WindowParentingClient {
 public:
  explicit TestWindowParentingClient(Window* root_window);

  TestWindowParentingClient(const TestWindowParentingClient&) = delete;
  TestWindowParentingClient& operator=(const TestWindowParentingClient&) =
      delete;

  ~TestWindowParentingClient() override;

  void set_default_parent(Window* parent) { default_parent_ = parent; }

  // Overridden from client::WindowParentingClient:
  Window* GetDefaultParent(Window* window,
                           const gfx::Rect& bounds,
                           const int64_t display_id) override;

 private:
  raw_ptr<Window> root_window_;

  // If non-null this is returned from GetDefaultParent().
  raw_ptr<Window> default_parent_ = nullptr;
};

}  // namespace test
}  // namespace aura

#endif  // UI_AURA_TEST_TEST_WINDOW_PARENTING_CLIENT_H_
