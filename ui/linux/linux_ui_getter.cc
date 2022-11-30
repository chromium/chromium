// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/linux/linux_ui_getter.h"

namespace ui {

// static
LinuxUiGetter* LinuxUiGetter::instance_ = nullptr;

LinuxUiGetter::LinuxUiGetter() {
  // Tests reset this, so avoid DCHECKs.
  instance_ = this;
}

LinuxUiGetter::~LinuxUiGetter() {
  instance_ = nullptr;
}

}  // namespace ui
