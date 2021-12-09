// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/inspect/ax_transform_mac.h"

#include "base/strings/sys_string_conversions.h"
#include "ui/accessibility/platform/inspect/ax_inspect_utils.h"

namespace ui {

constexpr char kHeightDictKey[] = "h";
constexpr char kNilValue[] = "_const_NULL";
constexpr char kRangeLenDictKey[] = "len";
constexpr char kRangeLocDictKey[] = "loc";
constexpr char kWidthDictKey[] = "w";
constexpr char kXCoordDictKey[] = "x";
constexpr char kYCoordDictKey[] = "y";

base::Value AXNSObjectToBaseValue(id value, const AXTreeIndexerMac* indexer) {
  if (!value) {
    return AXNilToBaseValue();
  }

  // NSArray
  if ([value isKindOfClass:[NSArray class]]) {
    return AXNSArrayToBaseValue((NSArray*)value, indexer);
  }

  // NSNumber
  if ([value isKindOfClass:[NSNumber class]]) {
    return base::Value([value intValue]);
  }

  // NSRange, NSSize
  if ([value isKindOfClass:[NSValue class]]) {
    if (0 == strcmp([value objCType], @encode(NSRange))) {
      return AXNSRangeToBaseValue([value rangeValue]);
    }
    if (0 == strcmp([value objCType], @encode(NSSize))) {
      return AXNSSizeToBaseValue([value sizeValue]);
    }
  }

  // AXValue
  if (CFGetTypeID(value) == AXValueGetTypeID()) {
    AXValueType type = AXValueGetType(static_cast<AXValueRef>(value));
    switch (type) {
      case kAXValueCGPointType: {
        NSPoint point;
        if (AXValueGetValue(static_cast<AXValueRef>(value), type, &point)) {
          return AXNSPointToBaseValue(point);
        }
      } break;
      case kAXValueCGSizeType: {
        NSSize size;
        if (AXValueGetValue(static_cast<AXValueRef>(value), type, &size)) {
          return AXNSSizeToBaseValue(size);
        }
      } break;
      case kAXValueCGRectType: {
        NSRect rect;
        if (AXValueGetValue(static_cast<AXValueRef>(value), type, &rect)) {
          return AXNSRectToBaseValue(rect);
        }
      } break;
      case kAXValueCFRangeType: {
        NSRange range;
        if (AXValueGetValue(static_cast<AXValueRef>(value), type, &range)) {
          return AXNSRangeToBaseValue(range);
        }
      } break;
      default:
        break;
    }
  }

  // Accessible object
  if (IsNSAccessibilityElement(value) || IsAXUIElement(value)) {
    return AXElementToBaseValue(value, indexer);
  }

  // Scalar value.
  return base::Value(
      base::SysNSStringToUTF16([NSString stringWithFormat:@"%@", value]));
}

base::Value AXElementToBaseValue(id node, const AXTreeIndexerMac* indexer) {
  return base::Value(AXMakeConst(indexer->IndexBy(node)));
}

base::Value AXNilToBaseValue() {
  return base::Value(kNilValue);
}

base::Value AXNSArrayToBaseValue(NSArray* node_array,
                                 const AXTreeIndexerMac* indexer) {
  base::Value list(base::Value::Type::LIST);
  for (NSUInteger i = 0; i < [node_array count]; i++)
    list.Append(AXNSObjectToBaseValue([node_array objectAtIndex:i], indexer));
  return list;
}

base::Value AXNSPointToBaseValue(NSPoint point_value) {
  base::Value point(base::Value::Type::DICTIONARY);
  point.SetIntPath(kXCoordDictKey, static_cast<int>(point_value.x));
  point.SetIntPath(kYCoordDictKey, static_cast<int>(point_value.y));
  return point;
}

base::Value AXNSSizeToBaseValue(NSSize size_value) {
  base::Value size(base::Value::Type::DICTIONARY);
  size.SetIntPath(AXMakeOrderedKey(kWidthDictKey, 0),
                  static_cast<int>(size_value.width));
  size.SetIntPath(AXMakeOrderedKey(kHeightDictKey, 1),
                  static_cast<int>(size_value.height));
  return size;
}

base::Value AXNSRectToBaseValue(NSRect rect_value) {
  base::Value rect(base::Value::Type::DICTIONARY);
  rect.SetIntPath(AXMakeOrderedKey(kXCoordDictKey, 0),
                  static_cast<int>(rect_value.origin.x));
  rect.SetIntPath(AXMakeOrderedKey(kYCoordDictKey, 1),
                  static_cast<int>(rect_value.origin.y));
  rect.SetIntPath(AXMakeOrderedKey(kWidthDictKey, 2),
                  static_cast<int>(rect_value.size.width));
  rect.SetIntPath(AXMakeOrderedKey(kHeightDictKey, 3),
                  static_cast<int>(rect_value.size.height));
  return rect;
}

base::Value AXNSRangeToBaseValue(NSRange node_range) {
  base::Value range(base::Value::Type::DICTIONARY);
  range.SetIntPath(AXMakeOrderedKey(kRangeLocDictKey, 0),
                   static_cast<int>(node_range.location));
  range.SetIntPath(AXMakeOrderedKey(kRangeLenDictKey, 1),
                   static_cast<int>(node_range.length));
  return range;
}

}  // namespace ui
