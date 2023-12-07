// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/ax_platform.h"

#include "base/check_op.h"

namespace ui {

namespace {

AXPlatform* g_instance = nullptr;

}  // namespace

// static
AXPlatform& AXPlatform::GetInstance() {
  CHECK_NE(g_instance, nullptr);
  return *g_instance;
}

AXPlatform::AXPlatform(Delegate& delegate) : delegate_(delegate) {
  DCHECK_EQ(g_instance, nullptr);
  g_instance = this;
}

AXPlatform::~AXPlatform() {
  DCHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

}  // namespace ui
