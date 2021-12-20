// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/ax_utils_mac.h"

#include "base/mac/scoped_cftyperef.h"
#include "ui/accessibility/ax_range.h"

// The following are private accessibility APIs required for cursor navigation
// and text selection. VoiceOver started relying on them in Mac OS X 10.11.
// They are public as of the 12.0 SDK.
#if !defined(MAC_OS_VERSION_12_0) || \
    MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_VERSION_12_0
using AXTextMarkerRangeRef = CFTypeRef;
using AXTextMarkerRef = CFTypeRef;
extern "C" {
CFTypeID AXTextMarkerGetTypeID();
AXTextMarkerRef AXTextMarkerCreate(CFAllocatorRef,
                                   const UInt8* bytes,
                                   CFIndex length);
size_t AXTextMarkerGetLength(AXTextMarkerRef);
const UInt8* AXTextMarkerGetBytePtr(AXTextMarkerRef);

CFTypeID AXTextMarkerRangeGetTypeID();
AXTextMarkerRef AXTextMarkerRangeCopyStartMarker(AXTextMarkerRangeRef);
AXTextMarkerRef AXTextMarkerRangeCopyEndMarker(AXTextMarkerRangeRef);
}  // extern "C"
#endif

namespace ui {

bool IsAXTextMarker(id object) {
  if (object == nil)
    return false;

  AXTextMarkerRef cf_text_marker = static_cast<AXTextMarkerRef>(object);
  DCHECK(cf_text_marker);
  return CFGetTypeID(cf_text_marker) == AXTextMarkerGetTypeID();
}

bool IsAXTextMarkerRange(id object) {
  if (object == nil)
    return false;

  AXTextMarkerRangeRef cf_marker_range =
      static_cast<AXTextMarkerRangeRef>(object);
  DCHECK(cf_marker_range);
  return CFGetTypeID(cf_marker_range) == AXTextMarkerRangeGetTypeID();
}

AXPlatformNodeDelegate::AXPosition AXTextMarkerToAXPosition(id text_marker) {
  if (!IsAXTextMarker(text_marker))
    return AXNodePosition::CreateNullPosition();

  AXTextMarkerRef cf_text_marker = static_cast<AXTextMarkerRef>(text_marker);
  if (AXTextMarkerGetLength(cf_text_marker) !=
      sizeof(AXPlatformNodeDelegate::SerializedPosition))
    return AXNodePosition::CreateNullPosition();

  const UInt8* source_buffer = AXTextMarkerGetBytePtr(cf_text_marker);
  if (!source_buffer)
    return AXNodePosition::CreateNullPosition();

  return AXNodePosition::Unserialize(
      *reinterpret_cast<const AXPlatformNodeDelegate::SerializedPosition*>(
          source_buffer));
}

AXPlatformNodeDelegate::AXRange AXTextMarkerRangeToAXRange(
    id text_marker_range) {
  if (!IsAXTextMarkerRange(text_marker_range)) {
    return AXPlatformNodeDelegate::AXRange();
  }

  AXTextMarkerRangeRef cf_marker_range =
      static_cast<AXTextMarkerRangeRef>(text_marker_range);

  base::ScopedCFTypeRef<AXTextMarkerRef> start_marker(
      AXTextMarkerRangeCopyStartMarker(cf_marker_range));
  base::ScopedCFTypeRef<AXTextMarkerRef> end_marker(
      AXTextMarkerRangeCopyEndMarker(cf_marker_range));
  if (!start_marker.get() || !end_marker.get())
    return AXPlatformNodeDelegate::AXRange();

  // |AXPlatformNodeDelegate::AXRange| takes ownership of its anchor and focus.
  AXPlatformNodeDelegate::AXPosition anchor =
      AXTextMarkerToAXPosition(static_cast<id>(start_marker.get()));
  AXPlatformNodeDelegate::AXPosition focus =
      AXTextMarkerToAXPosition(static_cast<id>(end_marker.get()));
  return AXPlatformNodeDelegate::AXRange(std::move(anchor), std::move(focus));
}

}  // namespace ui
