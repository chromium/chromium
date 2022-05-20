// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPCZ_SRC_REFERENCE_DRIVERS_OS_HANDLE_H_
#define IPCZ_SRC_REFERENCE_DRIVERS_OS_HANDLE_H_

#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include "reference_drivers/os_handle_win.h"
#elif BUILDFLAG(IS_MAC)
#include "reference_drivers/os_handle_mac.h"
#elif BUILDFLAG(IS_FUCHSIA)
#include "reference_drivers/os_handle_fuchsia.h"
#elif BUILDFLAG(IS_POSIX)
#include "reference_drivers/os_handle_posix.h"
#else
#error "Unsupported platform"
#endif

#endif  // IPCZ_SRC_REFERENCE_DRIVERS_OS_HANDLE_H_
