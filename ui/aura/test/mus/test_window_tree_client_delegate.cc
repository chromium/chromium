// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/test/mus/test_window_tree_client_delegate.h"

#include "ui/aura/mus/property_converter.h"
#include "ui/aura/mus/window_tree_host_mus.h"

namespace aura {

TestWindowTreeClientDelegate::TestWindowTreeClientDelegate()
    : property_converter_(std::make_unique<PropertyConverter>()) {}

TestWindowTreeClientDelegate::~TestWindowTreeClientDelegate() = default;

void TestWindowTreeClientDelegate::OnEmbed(
    std::unique_ptr<WindowTreeHostMus> window_tree_host) {}

void TestWindowTreeClientDelegate::OnUnembed(Window* root) {}

void TestWindowTreeClientDelegate::OnEmbedRootDestroyed(
    WindowTreeHostMus* window_tree_host) {}

void TestWindowTreeClientDelegate::OnLostConnection(WindowTreeClient* client) {}

PropertyConverter* TestWindowTreeClientDelegate::GetPropertyConverter() {
  return property_converter_.get();
}

}  // namespace aura
