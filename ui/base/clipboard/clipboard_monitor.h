// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_CLIPBOARD_CLIPBOARD_MONITOR_H_
#define UI_BASE_CLIPBOARD_CLIPBOARD_MONITOR_H_

#include "base/component_export.h"
#include "base/no_destructor.h"
#include "base/observer_list.h"
#include "base/threading/thread_checker.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

namespace ui {

class ClipboardObserver;

// A singleton instance to monitor and notify ClipboardObservers for clipboard
// changes.
class COMPONENT_EXPORT(UI_BASE_CLIPBOARD) ClipboardMonitor {
 public:
  static ClipboardMonitor* GetInstance();

  ClipboardMonitor(const ClipboardMonitor&) = delete;
  ClipboardMonitor& operator=(const ClipboardMonitor&) = delete;

  // Adds an observer.
  void AddObserver(ClipboardObserver* observer);

  // Removes an observer.
  void RemoveObserver(ClipboardObserver* observer);

  // Notifies all observers for clipboard data change.
  virtual void NotifyClipboardDataChanged();

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Notifies all observers for clipboard data read.
  virtual void NotifyClipboardDataRead();
#endif

 private:
  friend class base::NoDestructor<ClipboardMonitor>;

  ClipboardMonitor();
  virtual ~ClipboardMonitor();

  base::ObserverList<ClipboardObserver>::Unchecked observers_;

  THREAD_CHECKER(thread_checker_);
};

}  // namespace ui

#endif  // UI_BASE_CLIPBOARD_CLIPBOARD_MONITOR_H_
