// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_WIN_CRASH_ID_HELPER_H_
#define UI_GFX_WIN_CRASH_ID_HELPER_H_

#include <string>
#include <vector>

#include "base/no_destructor.h"
#include "base/threading/platform_thread.h"
#include "components/crash/core/common/crash_key.h"
#include "ui/gfx/gfx_export.h"

namespace gfx {

// CrashIdHelper is used to log (in crash dumps) the id(s) of the window/widget
// currently dispatching an event. Often times crashes occur purely in ui
// code, while the bug lies in client code. Logging an id helps better identify
// the client code that created the window/widget.
//
// This class only logs ids on the thread identified by RegisterMainThread().
//
// Example usage:
// {
//   auto logger = CrashIdHelper::Get()->OnWillProcessMessages(crash_id);
//   <do message processing>
// }
class GFX_EXPORT CrashIdHelper {
 public:
  static CrashIdHelper* Get();

  CrashIdHelper(const CrashIdHelper&) = delete;
  CrashIdHelper& operator=(const CrashIdHelper&) = delete;

  // Registers the thread used for logging.
  static void RegisterMainThread(base::PlatformThreadId thread_id);

  // RAII style class that unregisters in the destructor.
  class GFX_EXPORT ScopedLogger {
   public:
    ScopedLogger(const ScopedLogger&) = delete;
    ScopedLogger& operator=(const ScopedLogger&) = delete;

    ~ScopedLogger();

   private:
    friend class CrashIdHelper;
    ScopedLogger();
  };

  // Adds |id| to the list of active debugging ids. When the returned object
  // is destroyed, |id| is removed from the list of active debugging ids.
  // Returns null if logging is not enabled on the current thread.
  std::unique_ptr<ScopedLogger> OnWillProcessMessages(const std::string& id);

 private:
  friend base::NoDestructor<CrashIdHelper>;
  friend class CrashIdHelperTest;

  CrashIdHelper();
  ~CrashIdHelper();

  // Called from ~ScopedLogger. Removes the most recently added id.
  void OnDidProcessMessages();

  // Returns the identifier to put in the crash key.
  std::string CurrentCrashId() const;

  // Ordered list of debugging identifiers added.
  std::vector<std::string> ids_;

  // Set to true once |ids_| has more than one object, and false once |ids_| is
  // empty. That is, this is true once processing the windows message resulted
  // in processing another windows message (nested message loops). See comment
  // in implementation of CurrentCrashId() as to why this is tracked.
  bool was_nested_ = false;

  // This uses the name 'widget' as this code is most commonly triggered from
  // views, which uses the term Widget.
  crash_reporter::CrashKeyString<128> debugging_crash_key_{"widget-id"};

  static base::PlatformThreadId main_thread_id_;
};

}  // namespace gfx

#endif  // UI_GFX_WIN_CRASH_ID_HELPER_H_
