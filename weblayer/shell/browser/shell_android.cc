// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/shell/browser/shell.h"

namespace weblayer {

// Shell is only used on Android for weblayer_browsertests. So no need to
// implement these methods.
void Shell::PlatformInitialize(const gfx::Size& default_window_size) {}

void Shell::PlatformExit() {}

void Shell::PlatformCleanUp() {}

void Shell::PlatformEnableUIControl(UIControl control, bool is_enabled) {}

void Shell::PlatformSetAddressBarURL(const GURL& url) {}

void Shell::PlatformSetLoadProgress(double progress) {}

void Shell::PlatformCreateWindow(int width, int height) {}

void Shell::PlatformSetContents() {}

void Shell::PlatformResizeSubViews() {}

void Shell::Close() {}

void Shell::PlatformSetTitle(const base::string16& title) {}

}  // namespace weblayer
