// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPCZ_SRC_REFERENCE_DRIVERS_OS_HANDLE_WIN_H_
#define IPCZ_SRC_REFERENCE_DRIVERS_OS_HANDLE_WIN_H_

#include <windows.h>

namespace ipcz::reference_drivers {

// The Windows OSHandle implementation can wrap any HANDLE value.
class OSHandle {
 public:
  OSHandle();
  explicit OSHandle(HANDLE handle);

  OSHandle(const OSHandle&) = delete;
  OSHandle& operator=(const OSHandle&) = delete;

  OSHandle(OSHandle&& other);
  OSHandle& operator=(OSHandle&& other);

  ~OSHandle();

  void reset();

  // Duplicates the underlying handle, returning a new OSHandle to wrap it. The
  // handle must be valid.
  OSHandle Clone() const;

  bool is_valid() const { return handle_ != INVALID_HANDLE_VALUE; }

  HANDLE handle() const { return handle_; }

 private:
  HANDLE handle_ = INVALID_HANDLE_VALUE;
};

}  // namespace ipcz::reference_drivers

#endif  // IPCZ_SRC_REFERENCE_DRIVERS_OS_HANDLE_WIN_H_
