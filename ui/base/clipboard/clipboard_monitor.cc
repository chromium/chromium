// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/clipboard_monitor.h"

#include "base/no_destructor.h"
#include "base/observer_list.h"
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
  observers_.Notify(&ClipboardObserver::OnClipboardDataChanged);
}

#if BUILDFLAG(IS_CHROMEOS)
void ClipboardMonitor::NotifyClipboardDataRead() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  observers_.Notify(&ClipboardObserver::OnClipboardDataRead);
}
#endif

void ClipboardMonitor::AddObserver(ClipboardObserver* observer) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  bool should_start_notifying = false;
  if (observers_.empty()) {
    should_start_notifying = true;
  }
  observers_.AddObserver(observer);
  if (should_start_notifying && notifier_) {
    notifier_.get()->StartNotifying();
  }
}

void ClipboardMonitor::RemoveObserver(ClipboardObserver* observer) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  observers_.RemoveObserver(observer);
  if (observers_.empty() && notifier_) {
    notifier_.get()->StopNotifying();
  }
}

void ClipboardMonitor::SetNotifier(ClipboardChangeNotifier* source) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  notifier_ = source;
  if (notifier_ && !observers_.empty()) {
    notifier_.get()->StartNotifying();
  }
}

}  // namespace ui
