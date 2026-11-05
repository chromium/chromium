// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/shortcut_mapping_pref_delegate.h"

#include "base/check.h"
#include "base/check_op.h"

namespace ui {

// static
ShortcutMappingPrefDelegate* ShortcutMappingPrefDelegate::instance_ = nullptr;

// static
ShortcutMappingPrefDelegate* ShortcutMappingPrefDelegate::GetInstance() {
  return instance_;
}

// static
bool ShortcutMappingPrefDelegate::IsInitialized() {
  return instance_ != nullptr;
}

ShortcutMappingPrefDelegate::ShortcutMappingPrefDelegate() {
  CHECK(!instance_, base::NotFatalUntil::M163);
  instance_ = this;
}

ShortcutMappingPrefDelegate::~ShortcutMappingPrefDelegate() {
  CHECK_EQ(instance_, this, base::NotFatalUntil::M163);
  instance_ = nullptr;
}

}  // namespace ui
