// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_TEST_MUS_TEST_WINDOW_TREE_CLIENT_DELEGATE_H_
#define UI_AURA_TEST_MUS_TEST_WINDOW_TREE_CLIENT_DELEGATE_H_

#include <memory>

#include "ui/aura/mus/window_tree_client_delegate.h"

namespace aura {

class PropertyConverter;

class TestWindowTreeClientDelegate : public WindowTreeClientDelegate {
 public:
  TestWindowTreeClientDelegate();
  ~TestWindowTreeClientDelegate() override;

  // WindowTreeClientDelegate:
  void OnEmbed(std::unique_ptr<WindowTreeHostMus> window_tree_host) override;
  void OnUnembed(Window* root) override;
  void OnEmbedRootDestroyed(WindowTreeHostMus* window_tree_host) override;
  void OnLostConnection(WindowTreeClient* client) override;
  PropertyConverter* GetPropertyConverter() override;

 private:
  std::unique_ptr<PropertyConverter> property_converter_;

  DISALLOW_COPY_AND_ASSIGN(TestWindowTreeClientDelegate);
};

}  // namespace aura

#endif  // UI_AURA_TEST_MUS_TEST_WINDOW_TREE_CLIENT_DELEGATE_H_
