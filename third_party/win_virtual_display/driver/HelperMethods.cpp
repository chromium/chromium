// Copyright (c) Microsoft Corporation

#include "HelperMethods.h"

namespace display::test {
inline void FillSignalInfo(DISPLAYCONFIG_VIDEO_SIGNAL_INFO& Mode,
                           DWORD Width,
                           DWORD Height,
                           DWORD VSync,
                           bool bMonitorMode) {
  Mode.totalSize.cx = Mode.activeSize.cx = Width;
  Mode.totalSize.cy = Mode.activeSize.cy = Height;

  // See
  // https://docs.microsoft.com/en-us/windows/win32/api/wingdi/ns-wingdi-displayconfig_video_signal_info
  Mode.AdditionalSignalInfo.vSyncFreqDivider = bMonitorMode ? 0 : 1;
  Mode.AdditionalSignalInfo.videoStandard = 255;

  Mode.vSyncFreq.Numerator = VSync;
  Mode.vSyncFreq.Denominator = 1;
  Mode.hSyncFreq.Numerator = VSync * Height;
  Mode.hSyncFreq.Denominator = 1;

  Mode.scanLineOrdering = DISPLAYCONFIG_SCANLINE_ORDERING_PROGRESSIVE;

  Mode.pixelRate = ((UINT64)VSync) * ((UINT64)Width) * ((UINT64)Height);
}

IDDCX_MONITOR_MODE CreateIddCxMonitorMode(
    DWORD Width,
    DWORD Height,
    DWORD VSync,
    IDDCX_MONITOR_MODE_ORIGIN Origin = IDDCX_MONITOR_MODE_ORIGIN_DRIVER) {
  IDDCX_MONITOR_MODE Mode = {};

  Mode.Size = sizeof(Mode);
  Mode.Origin = Origin;
  FillSignalInfo(Mode.MonitorVideoSignalInfo, Width, Height, VSync, true);

  return Mode;
}

IDDCX_TARGET_MODE CreateIddCxTargetMode(DWORD Width,
                                        DWORD Height,
                                        DWORD VSync) {
  IDDCX_TARGET_MODE Mode = {};

  Mode.Size = sizeof(Mode);
  FillSignalInfo(Mode.TargetVideoSignalInfo.targetVideoSignalInfo, Width,
                 Height, VSync, false);

  return Mode;
}
}  // namespace display::test
