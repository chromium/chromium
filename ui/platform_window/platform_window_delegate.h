// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_PLATFORM_WINDOW_PLATFORM_WINDOW_DELEGATE_H_
#define UI_PLATFORM_WINDOW_PLATFORM_WINDOW_DELEGATE_H_

#include "build/build_config.h"

// By default, PlatformWindowDelegateBase is used. However, different platforms
// should specify what delegate they would like to use if needed.
#if defined(OS_LINUX)
#include "ui/platform_window/platform_window_delegate_linux.h"
#else
#include "ui/platform_window/platform_window_delegate_base.h"
#endif

namespace ui {

#if defined(OS_LINUX)
using PlatformWindowDelegate = PlatformWindowDelegateLinux;
#else
using PlatformWindowDelegate = PlatformWindowDelegateBase;
#endif

}  // namespace ui

#endif  // UI_PLATFORM_WINDOW_PLATFORM_WINDOW_DELEGATE_H_
