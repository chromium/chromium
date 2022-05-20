// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPCZ_SRC_REFERENCE_DRIVERS_OS_HANDLE_FUCHSIA_H_
#define IPCZ_SRC_REFERENCE_DRIVERS_OS_HANDLE_FUCHSIA_H_

#include <lib/zx/handle.h>

namespace ipcz::reference_drivers {

// The Fuchsia OSHandle implementation can wrap any zx::handle.
class OSHandle {
 public:
  OSHandle();
  explicit OSHandle(zx::handle handle);

  OSHandle(const OSHandle&) = delete;
  OSHandle& operator=(const OSHandle&) = delete;

  OSHandle(OSHandle&& other);
  OSHandle& operator=(OSHandle&& other);

  ~OSHandle();

  void reset();

  // Duplicates the underlying handle, returning a new OSHandle to wrap it. The
  // handle must be valid.
  OSHandle Clone() const;

  bool is_valid() const { return handle_.is_valid(); }
  const zx::handle& handle() const { return handle_; }

 private:
  zx::handle handle_;
};

}  // namespace ipcz::reference_drivers

#endif  // IPCZ_SRC_REFERENCE_DRIVERS_OS_HANDLE_FUCHSIA_H_
