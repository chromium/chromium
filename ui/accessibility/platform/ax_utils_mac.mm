// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/ax_utils_mac.h"

#include <CoreFoundation/CoreFoundation.h>
#include <Foundation/Foundation.h>

#include "base/apple/scoped_cftyperef.h"
#include "ui/accessibility/ax_range.h"
#include "ui/accessibility/platform/ax_platform_node_base.h"
#include "ui/accessibility/platform/ax_platform_node_cocoa.h"
#include "ui/accessibility/platform/ax_platform_node_delegate.h"

namespace ui {

bool IsAXTextMarker(id object) {
  if (object == nil) {
    return false;
  }
  return CFGetTypeID((__bridge CFTypeRef)object) == AXTextMarkerGetTypeID();
}

bool IsAXTextMarkerRange(id object) {
  if (object == nil) {
    return false;
  }
  return CFGetTypeID((__bridge CFTypeRef)object) ==
         AXTextMarkerRangeGetTypeID();
}

AXPlatformNodeDelegate::AXPosition AXTextMarkerToAXPosition(id text_marker) {
  if (!IsAXTextMarker(text_marker)) {
    return AXNodePosition::CreateNullPosition();
  }

  AXTextMarkerRef cf_text_marker = (__bridge AXTextMarkerRef)text_marker;
  if (AXTextMarkerGetLength(cf_text_marker) !=
      sizeof(AXPlatformNodeDelegate::SerializedPosition)) {
    return AXNodePosition::CreateNullPosition();
  }

  const UInt8* source_buffer = AXTextMarkerGetBytePtr(cf_text_marker);
  if (!source_buffer) {
    return AXNodePosition::CreateNullPosition();
  }

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
      (__bridge AXTextMarkerRangeRef)text_marker_range;

  id start_marker =
      CFBridgingRelease(AXTextMarkerRangeCopyStartMarker(cf_marker_range));
  id end_marker =
      CFBridgingRelease(AXTextMarkerRangeCopyEndMarker(cf_marker_range));
  if (!start_marker || !end_marker) {
    return AXPlatformNodeDelegate::AXRange();
  }

  // |AXPlatformNodeDelegate::AXRange| takes ownership of its anchor and focus.
  AXPlatformNodeDelegate::AXPosition anchor =
      AXTextMarkerToAXPosition(start_marker);
  AXPlatformNodeDelegate::AXPosition focus =
      AXTextMarkerToAXPosition(end_marker);
  return AXPlatformNodeDelegate::AXRange(std::move(anchor), std::move(focus));
}

id AXPositionToAXTextMarker(AXPlatformNodeDelegate::AXPosition position) {
  // AXTextMarkerCreate is a system function that makes a copy of the data
  // buffer given to it.
  AXPlatformNodeDelegate::SerializedPosition serialized = position->Serialize();
  return CFBridgingRelease(AXTextMarkerCreate(
      kCFAllocatorDefault, reinterpret_cast<const UInt8*>(&serialized),
      sizeof(AXPlatformNodeDelegate::SerializedPosition)));
}

id AXRangeToAXTextMarkerRange(AXPlatformNodeDelegate::AXRange range) {
  AXPlatformNodeDelegate::SerializedPosition serialized_anchor =
      range.anchor()->Serialize();
  AXPlatformNodeDelegate::SerializedPosition serialized_focus =
      range.focus()->Serialize();

  base::apple::ScopedCFTypeRef<AXTextMarkerRef> start_marker(AXTextMarkerCreate(
      kCFAllocatorDefault, reinterpret_cast<const UInt8*>(&serialized_anchor),
      sizeof(AXPlatformNodeDelegate::SerializedPosition)));
  base::apple::ScopedCFTypeRef<AXTextMarkerRef> end_marker(AXTextMarkerCreate(
      kCFAllocatorDefault, reinterpret_cast<const UInt8*>(&serialized_focus),
      sizeof(AXPlatformNodeDelegate::SerializedPosition)));

  return CFBridgingRelease(AXTextMarkerRangeCreate(
      kCFAllocatorDefault, start_marker.get(), end_marker.get()));
}

id AXTextMarkerFrom(AXPlatformNodeCocoa* anchor,
                    int offset,
                    ax::mojom::TextAffinity affinity) {
  AXPlatformNode* anchor_platform_node = anchor.node;
  AXPlatformNodeDelegate* anchor_node = anchor_platform_node->GetDelegate();
  AXPlatformNodeDelegate::AXPosition position =
      anchor_node->CreateTextPositionAt(offset, affinity);
  return AXPositionToAXTextMarker(std::move(position));
}

id AXTextMarkerRangeFrom(id start_textmarker, id end_textmarker) {
  return CFBridgingRelease(AXTextMarkerRangeCreate(
      kCFAllocatorDefault, (__bridge AXTextMarkerRef)start_textmarker,
      (__bridge AXTextMarkerRef)end_textmarker));
}

id AXTextMarkerRangeStart(id text_marker_range) {
  return CFBridgingRelease(AXTextMarkerRangeCopyStartMarker(
      (__bridge AXTextMarkerRangeRef)text_marker_range));
}

id AXTextMarkerRangeEnd(id text_marker_range) {
  return CFBridgingRelease(AXTextMarkerRangeCopyEndMarker(
      (__bridge AXTextMarkerRangeRef)text_marker_range));
}

}  // namespace ui
