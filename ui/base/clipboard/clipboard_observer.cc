// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/clipboard_observer.h"

#include "build/chromeos_buildflags.h"

namespace ui {

void ClipboardObserver::OnClipboardDataChanged() {}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void ClipboardObserver::OnClipboardDataRead() {}
#endif

ClipboardObserver::~ClipboardObserver() = default;

}  // namespace ui
