// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/clipboard_observer.h"

namespace ui {

void ClipboardObserver::OnClipboardDataChanged() {}

#if defined(OS_CHROMEOS)
void ClipboardObserver::OnClipboardDataRead() {}
#endif

ClipboardObserver::~ClipboardObserver() = default;

}  // namespace ui
