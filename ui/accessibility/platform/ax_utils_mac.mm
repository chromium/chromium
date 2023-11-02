// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/ax_utils_mac.h"

#include "base/mac/scoped_cftyperef.h"
#include "ui/accessibility/ax_range.h"
#include "ui/accessibility/platform/ax_platform_node_base.h"
#include "ui/accessibility/platform/ax_platform_node_cocoa.h"
#include "ui/accessibility/platform/ax_platform_node_delegate.h"

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
AXTextMarkerRangeRef AXTextMarkerRangeCreate(CFAllocatorRef,
                                             AXTextMarkerRef start,
                                             AXTextMarkerRef end);
AXTextMarkerRef AXTextMarkerRangeCopyStartMarker(AXTextMarkerRangeRef);
AXTextMarkerRef AXTextMarkerRangeCopyEndMarker(AXTextMarkerRangeRef);
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

id AXPositionToAXTextMarker(AXPlatformNodeDelegate::AXPosition position) {
  // AXTextMarkerCreate is a system function that makes a copy of the data
  // buffer given to it.
  AXPlatformNodeDelegate::SerializedPosition serialized = position->Serialize();
  AXTextMarkerRef cf_text_marker = AXTextMarkerCreate(
      kCFAllocatorDefault, reinterpret_cast<const UInt8*>(&serialized),
      sizeof(AXPlatformNodeDelegate::SerializedPosition));
  return [static_cast<id>(cf_text_marker) autorelease];
}

id AXRangeToAXTextMarkerRange(AXPlatformNodeDelegate::AXRange range) {
  AXPlatformNodeDelegate::SerializedPosition serialized_anchor =
      range.anchor()->Serialize();
  AXPlatformNodeDelegate::SerializedPosition serialized_focus =
      range.focus()->Serialize();

  base::ScopedCFTypeRef<AXTextMarkerRef> start_marker(AXTextMarkerCreate(
      kCFAllocatorDefault, reinterpret_cast<const UInt8*>(&serialized_anchor),
      sizeof(AXPlatformNodeDelegate::SerializedPosition)));
  base::ScopedCFTypeRef<AXTextMarkerRef> end_marker(AXTextMarkerCreate(
      kCFAllocatorDefault, reinterpret_cast<const UInt8*>(&serialized_focus),
      sizeof(AXPlatformNodeDelegate::SerializedPosition)));

  AXTextMarkerRangeRef cf_marker_range =
      AXTextMarkerRangeCreate(kCFAllocatorDefault, start_marker, end_marker);
  return [static_cast<id>(cf_marker_range) autorelease];
}

id AXTextMarkerFrom(const AXPlatformNodeCocoa* anchor,
                    int offset,
                    ax::mojom::TextAffinity affinity) {
  AXPlatformNode* anchor_platform_node = [static_cast<id>(anchor) node];
  AXPlatformNodeDelegate* anchor_node = anchor_platform_node->GetDelegate();
  AXPlatformNodeDelegate::AXPosition position =
      anchor_node->CreateTextPositionAt(offset, affinity);
  return AXPositionToAXTextMarker(std::move(position));
}

id AXTextMarkerRangeFrom(id start_textmarker, id end_textmarker) {
  AXTextMarkerRangeRef cf_marker_range = AXTextMarkerRangeCreate(
      kCFAllocatorDefault, static_cast<AXTextMarkerRef>(start_textmarker),
      static_cast<AXTextMarkerRef>(end_textmarker));
  return [static_cast<id>(cf_marker_range) autorelease];
}

id AXTextMarkerRangeStart(id text_marker_range) {
  return static_cast<id>(AXTextMarkerRangeCopyStartMarker(
      static_cast<AXTextMarkerRangeRef>(text_marker_range)));
}

id AXTextMarkerRangeEnd(id text_marker_range) {
  return static_cast<id>(AXTextMarkerRangeCopyEndMarker(
      static_cast<AXTextMarkerRangeRef>(text_marker_range)));
}

}  // namespace ui
