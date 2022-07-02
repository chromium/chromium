// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPCZ_SRC_REFERENCE_DRIVERS_SINGLE_PROCESS_REFERENCE_DRIVER_BASE_H_
#define IPCZ_SRC_REFERENCE_DRIVERS_SINGLE_PROCESS_REFERENCE_DRIVER_BASE_H_

#include "ipcz/ipcz.h"

namespace ipcz::reference_drivers {

// A partial IpczDriver providing implementation common to both the sync and
// and async single-process drivers.
extern const IpczDriver kSingleProcessReferenceDriverBase;

}  // namespace ipcz::reference_drivers

#endif  // IPCZ_SRC_REFERENCE_DRIVERS_SINGLE_PROCESS_REFERENCE_DRIVER_BASE_H_
