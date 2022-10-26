// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_WIN_AUDIO_EDID_SCAN_H_
#define UI_DISPLAY_WIN_AUDIO_EDID_SCAN_H_

#include <stdint.h>

#include "ui/display/display_export.h"

namespace display {
namespace win {

// The WMI service allows the querying of monitor-type devices which report
// Extended Display Identification Data (EDID).  The WMI service can be
// queried for a list of COM objects which represent the "paths" which
// are associated with individual EDID devices.  Querying each of those
// paths using the WmiGetMonitorRawEEdidV1Block method returns the EDID
// blocks for those devices.  We query the extended blocks which contain
// the Short Audio Descriptor (SAD), and parse them to obtain a bitmask
// indicating which audio content is supported.  The bitmask bits are
// defined in edid_parser.h, as returned from the EdidParser::audio_formats()
// method.  If multiple EDID devices are present, the intersection is
// reported as the bitmask.
DISPLAY_EXPORT uint32_t ScanEdidBitstreams();

}  // namespace win
}  // namespace display

#endif  // UI_DISPLAY_WIN_AUDIO_EDID_SCAN_H_
