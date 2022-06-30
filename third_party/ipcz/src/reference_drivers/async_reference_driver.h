// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPCZ_SRC_DRIVERS_ASYNC_REFERENCE_DRIVER_H_
#define IPCZ_SRC_DRIVERS_ASYNC_REFERENCE_DRIVER_H_

#include "ipcz/ipcz.h"

namespace ipcz::reference_drivers {

// An async driver for single-process tests. Each transport runs its own thread
// with a simple task queue. Transmission from a transport posts a task to its
// peer's queue. The resulting non-determinism effectively simulates a typical
// production driver, without the complexity of a multiprocess environment.
extern const IpczDriver kAsyncReferenceDriver;

}  // namespace ipcz::reference_drivers

#endif  // IPCZ_SRC_DRIVERS_ASYNC_REFERENCE_DRIVER_H_
