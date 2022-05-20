// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPCZ_SRC_REFERENCE_DRIVERS_OS_HANDLE_POSIX_H_
#define IPCZ_SRC_REFERENCE_DRIVERS_OS_HANDLE_POSIX_H_

namespace ipcz::reference_drivers {

// The POSIX OSHandle implementation wraps a file descriptor.
class OSHandle {
 public:
  OSHandle();
  explicit OSHandle(int fd);

  OSHandle(const OSHandle&) = delete;
  OSHandle& operator=(const OSHandle&) = delete;

  OSHandle(OSHandle&& other);
  OSHandle& operator=(OSHandle&& other);

  ~OSHandle();

  void reset();

  // Duplicates the underlying handle, returning a new OSHandle to wrap it. The
  // handle must be valid.
  OSHandle Clone() const;

  bool is_valid() const { return fd_ != -1; }

  int fd() const { return fd_; }

 private:
  int fd_ = -1;
};

}  // namespace ipcz::reference_drivers

#endif  // IPCZ_SRC_REFERENCE_DRIVERS_OS_HANDLE_POSIX_H_
