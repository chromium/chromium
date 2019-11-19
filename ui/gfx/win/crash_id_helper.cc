// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/win/crash_id_helper.h"

#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"

namespace gfx {

// static
CrashIdHelper* CrashIdHelper::Get() {
  static base::NoDestructor<CrashIdHelper> helper;
  return helper.get();
}

// static
void CrashIdHelper::RegisterMainThread(base::PlatformThreadId thread_id) {
  main_thread_id_ = thread_id;
}

CrashIdHelper::ScopedLogger::~ScopedLogger() {
  CrashIdHelper::Get()->OnDidProcessMessages();
}

CrashIdHelper::ScopedLogger::ScopedLogger() = default;

std::unique_ptr<CrashIdHelper::ScopedLogger>
CrashIdHelper::OnWillProcessMessages(const std::string& id) {
  if (main_thread_id_ == base::kInvalidThreadId ||
      base::PlatformThread::CurrentId() != main_thread_id_) {
    return nullptr;
  }

  if (!ids_.empty())
    was_nested_ = true;
  ids_.push_back(id.empty() ? "unspecified" : id);
  debugging_crash_key_.Set(CurrentCrashId());
  // base::WrapUnique() as constructor is private.
  return base::WrapUnique(new ScopedLogger);
}

void CrashIdHelper::OnDidProcessMessages() {
  DCHECK(!ids_.empty());
  ids_.pop_back();
  if (ids_.empty()) {
    debugging_crash_key_.Clear();
    was_nested_ = false;
  } else {
    debugging_crash_key_.Set(CurrentCrashId());
  }
}

CrashIdHelper::CrashIdHelper() = default;

CrashIdHelper::~CrashIdHelper() = default;

std::string CrashIdHelper::CurrentCrashId() const {
  // This should only be called when there is at least one id.
  DCHECK(!ids_.empty());
  // Common case is only one id.
  if (ids_.size() == 1) {
    // If the size of |ids_| is 1, then the message loop is not nested. If
    // |was_nested_| is true, it means in processing the message corresponding
    // to ids_[0] another message was processed, resulting in nested message
    // loops.  A nested message loop can lead to reentrancy and/or problems when
    // the stack unravels. For example, it's entirely possible that when a
    // nested message loop completes, objects further up in the stack have been
    // deleted. "(N)" is added to signify that a nested message loop was run at
    // some point during the current message loop.
    return was_nested_ ? "(N) " + ids_[0] : ids_[0];
  }
  return base::JoinString(ids_, ">");
}

// static
base::PlatformThreadId CrashIdHelper::main_thread_id_ = base::kInvalidThreadId;

}  // namespace gfx
