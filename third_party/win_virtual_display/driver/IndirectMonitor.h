// Copyright (c) Microsoft Corporation

#ifndef THIRD_PARTY_WIN_VIRTUAL_DISPLAY_DRIVER_INDIRECTMONITOR_H_
#define THIRD_PARTY_WIN_VIRTUAL_DISPLAY_DRIVER_INDIRECTMONITOR_H_

// Make sure we don't get min/max macros
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>

#include <vector>
#include "Edid.h"

namespace Windows {
// Represents a virtual monitor, encapsulates an EDID and modes.
struct IndirectSampleMonitor {
  static constexpr size_t szModeList = 3;
  BYTE pEdidBlock[Edid::kBlockSize];
  struct SampleMonitorMode {
    DWORD Width;
    DWORD Height;
    DWORD VSync;
  };
  std::vector<SampleMonitorMode> pModeList;
  DWORD ulPreferredModeIdx = 0;
};
}  // namespace Windows

#endif  // THIRD_PARTY_WIN_VIRTUAL_DISPLAY_DRIVER_INDIRECTMONITOR_H_
