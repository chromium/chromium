// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/test_zaura_output_manager.h"

#include <aura-shell-server-protocol.h>

namespace wl {

namespace {
constexpr uint32_t kZAuraOutputManagerVersion = 1;

}  // namespace

TestZAuraOutputManager::TestZAuraOutputManager()
    : GlobalObject(&zaura_output_manager_interface,
                   nullptr,
                   kZAuraOutputManagerVersion) {}

TestZAuraOutputManager::~TestZAuraOutputManager() = default;

}  // namespace wl
