// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/clipboard_monitor.h"

#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/clipboard_observer.h"

namespace ui {

ClipboardMonitor::ClipboardMonitor() = default;

ClipboardMonitor::~ClipboardMonitor() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

// static
ClipboardMonitor* ClipboardMonitor::GetInstance() {
  static base::NoDestructor<ClipboardMonitor> monitor;
  return monitor.get();
}

void ClipboardMonitor::NotifyClipboardDataChanged() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  for (ClipboardObserver& observer : observers_)
    observer.OnClipboardDataChanged();
}

void ClipboardMonitor::AddObserver(ClipboardObserver* observer) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  observers_.AddObserver(observer);
}

void ClipboardMonitor::RemoveObserver(ClipboardObserver* observer) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  observers_.RemoveObserver(observer);
}

}  // namespace ui
