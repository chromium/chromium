// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/accessibility/platform/inspect/ax_transform_mac.h"

#include "base/apple/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#include "ui/accessibility/ax_range.h"
#include "ui/accessibility/platform/ax_platform_node.h"
#include "ui/accessibility/platform/ax_platform_node_cocoa.h"
#include "ui/accessibility/platform/ax_platform_node_delegate.h"
#include "ui/accessibility/platform/ax_platform_tree_manager.h"
#include "ui/accessibility/platform/ax_utils_mac.h"
#include "ui/accessibility/platform/inspect/ax_element_wrapper_mac.h"
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
  if (base::apple::ObjCCast<NSArray>(value)) {
    return base::Value(AXNSArrayToBaseValue(value, indexer));
  }

  // AXCustomContent
  if (AXCustomContent* custom_content =
          base::apple::ObjCCast<AXCustomContent>(value)) {
    return base::Value(AXCustomContentToBaseValue(custom_content));
  }

  // NSDictionary
  if (NSDictionary* dictionary = base::apple::ObjCCast<NSDictionary>(value)) {
    return base::Value(AXNSDictionaryToBaseValue(dictionary, indexer));
  }

  // NSNumber
  if (NSNumber* number = base::apple::ObjCCast<NSNumber>(value)) {
    return base::Value(number.intValue);
  }

  // NSRange, NSSize
  if (NSValue* ns_value = base::apple::ObjCCast<NSValue>(value)) {
    if (0 == strcmp(ns_value.objCType, @encode(NSRange))) {
      return base::Value(AXNSRangeToBaseValue(ns_value.rangeValue));
    }
    if (0 == strcmp(ns_value.objCType, @encode(NSSize))) {
      return base::Value(AXNSSizeToBaseValue(ns_value.sizeValue));
    }
  }

  // NSAttributedString
  if (NSAttributedString* attr_string =
          base::apple::ObjCCast<NSAttributedString>(value)) {
    return NSAttributedStringToBaseValue(attr_string, indexer);
  }

  // CGColorRef
  if (CFGetTypeID((__bridge CFTypeRef)value) == CGColorGetTypeID()) {
    return base::Value(CGColorRefToBaseValue((__bridge CGColorRef)value));
  }

  // AXValue
  if (CFGetTypeID((__bridge CFTypeRef)value) == AXValueGetTypeID()) {
    AXValueRef ax_value = (__bridge AXValueRef)value;
    AXValueType type = AXValueGetType(ax_value);
    switch (type) {
      case kAXValueCGPointType: {
        NSPoint point;
        if (AXValueGetValue(ax_value, type, &point)) {
          return base::Value(AXNSPointToBaseValue(point));
        }
      } break;
      case kAXValueCGSizeType: {
        NSSize size;
        if (AXValueGetValue(ax_value, type, &size)) {
          return base::Value(AXNSSizeToBaseValue(size));
        }
      } break;
      case kAXValueCGRectType: {
        NSRect rect;
        if (AXValueGetValue(ax_value, type, &rect)) {
          return base::Value(AXNSRectToBaseValue(rect));
        }
      } break;
      case kAXValueCFRangeType: {
        NSRange range;
        if (AXValueGetValue(ax_value, type, &range)) {
          return base::Value(AXNSRangeToBaseValue(range));
        }
      } break;
      default:
        break;
    }
  }

  // AXTextMarker
  if (IsAXTextMarker(value)) {
    return AXTextMarkerToBaseValue(value, indexer);
  }

  // AXTextMarkerRange
  if (IsAXTextMarkerRange(value)) {
    return AXTextMarkerRangeToBaseValue(value, indexer);
  }

  // Accessible object
  if (AXElementWrapper::IsValidElement(value)) {
    return AXElementToBaseValue(value, indexer);
  }

  // Scalar value.
  return base::Value(
      base::SysNSStringToUTF16([NSString stringWithFormat:@"%@", value]));
}

base::Value AXElementToBaseValue(id node, const AXTreeIndexerMac* indexer) {
  return base::Value(AXMakeConst(indexer->IndexBy(node)));
}

base::Value AXPositionToBaseValue(
    const AXPlatformNodeDelegate::AXPosition& position,
    const AXTreeIndexerMac* indexer) {
  if (position->IsNullPosition()) {
    return AXNilToBaseValue();
  }

  const AXPlatformTreeManager* manager =
      static_cast<AXPlatformTreeManager*>(position->GetManager());
  if (!manager) {
    return AXNilToBaseValue();
  }

  AXPlatformNode* platform_node_anchor =
      manager->GetPlatformNodeFromTree(position->anchor_id());
  if (!platform_node_anchor) {
    return AXNilToBaseValue();
  }

  AXPlatformNodeCocoa* cocoa_anchor = static_cast<AXPlatformNodeCocoa*>(
      platform_node_anchor->GetNativeViewAccessible());
  if (!cocoa_anchor) {
    return AXNilToBaseValue();
  }

  std::string affinity;
  switch (position->affinity()) {
    case ax::mojom::TextAffinity::kNone:
      affinity = "none";
      break;
    case ax::mojom::TextAffinity::kDownstream:
      affinity = "down";
      break;
    case ax::mojom::TextAffinity::kUpstream:
      affinity = "up";
      break;
  }

  base::Value::Dict value =
      base::Value::Dict()
          .Set(AXMakeSetKey(AXMakeOrderedKey("anchor", 0)),
               AXElementToBaseValue(static_cast<id>(cocoa_anchor), indexer))
          .Set(AXMakeSetKey(AXMakeOrderedKey("offset", 1)),
               position->text_offset())
          .Set(AXMakeSetKey(AXMakeOrderedKey("affinity", 2)),
               AXMakeConst(affinity));
  return base::Value(std::move(value));
}

base::Value AXTextMarkerToBaseValue(id text_marker,
                                    const AXTreeIndexerMac* indexer) {
  return AXPositionToBaseValue(AXTextMarkerToAXPosition(text_marker), indexer);
}

base::Value AXTextMarkerRangeToBaseValue(id text_marker_range,
                                         const AXTreeIndexerMac* indexer) {
  AXPlatformNodeDelegate::AXRange ax_range =
      AXTextMarkerRangeToAXRange(text_marker_range);
  if (ax_range.IsNull()) {
    return AXNilToBaseValue();
  }

  base::Value::Dict value =
      base::Value::Dict()
          .Set("anchor",
               AXPositionToBaseValue(ax_range.anchor()->Clone(), indexer))
          .Set("focus",
               AXPositionToBaseValue(ax_range.focus()->Clone(), indexer));
  return base::Value(std::move(value));
}

base::Value NSAttributedStringToBaseValue(NSAttributedString* attr_string,
                                          const AXTreeIndexerMac* indexer) {
  __block base::Value::Dict result;

  [attr_string
      enumerateAttributesInRange:NSMakeRange(0, attr_string.length)
                         options:
                             NSAttributedStringEnumerationLongestEffectiveRangeNotRequired
                      usingBlock:^(NSDictionary* attrs, NSRange nsRange,
                                   BOOL* stop) {
                        __block base::Value::Dict base_attrs;
                        [attrs enumerateKeysAndObjectsUsingBlock:^(
                                   NSString* key, id attr, BOOL* dict_stop) {
                          base_attrs.Set(
                              std::string(base::SysNSStringToUTF8(key)),
                              AXNSObjectToBaseValue(attr, indexer));
                        }];

                        result.Set(std::string(base::SysNSStringToUTF8(
                                       [attr_string.string
                                           substringWithRange:nsRange])),
                                   std::move(base_attrs));
                      }];
  return base::Value(std::move(result));
}

base::Value CGColorRefToBaseValue(CGColorRef color) {
  const CGFloat* color_components = CGColorGetComponents(color);
  return base::Value(base::SysNSStringToUTF16(
      [NSString stringWithFormat:@"CGColor(%1.2f, %1.2f, %1.2f, %1.2f)",
                                 color_components[0], color_components[1],
                                 color_components[2], color_components[3]]));
}

base::Value AXNilToBaseValue() {
  return base::Value(kNilValue);
}

base::Value::List AXNSArrayToBaseValue(NSArray* node_array,
                                       const AXTreeIndexerMac* indexer) {
  base::Value::List list;
  for (id item in node_array) {
    list.Append(AXNSObjectToBaseValue(item, indexer));
  }
  return list;
}

base::Value::Dict AXCustomContentToBaseValue(AXCustomContent* content) {
  base::Value::Dict value =
      base::Value::Dict()
          .Set("label", base::SysNSStringToUTF16(content.label))
          .Set("value", base::SysNSStringToUTF16(content.value));
  return value;
}

base::Value::Dict AXNSDictionaryToBaseValue(NSDictionary* dictionary_value,
                                            const AXTreeIndexerMac* indexer) {
  base::Value::Dict dictionary;
  for (NSString* key in dictionary_value) {
    dictionary.SetByDottedPath(
        base::SysNSStringToUTF8(key),
        AXNSObjectToBaseValue(dictionary_value[key], indexer));
  }
  return dictionary;
}

base::Value::Dict AXNSPointToBaseValue(NSPoint point_value) {
  base::Value::Dict point =
      base::Value::Dict()
          .Set(kXCoordDictKey, static_cast<int>(point_value.x))
          .Set(kYCoordDictKey, static_cast<int>(point_value.y));
  return point;
}

base::Value::Dict AXNSSizeToBaseValue(NSSize size_value) {
  base::Value::Dict size = base::Value::Dict()
                               .Set(AXMakeOrderedKey(kWidthDictKey, 0),
                                    static_cast<int>(size_value.width))
                               .Set(AXMakeOrderedKey(kHeightDictKey, 1),
                                    static_cast<int>(size_value.height));
  return size;
}

base::Value::Dict AXNSRectToBaseValue(NSRect rect_value) {
  base::Value::Dict rect = base::Value::Dict()
                               .Set(AXMakeOrderedKey(kXCoordDictKey, 0),
                                    static_cast<int>(rect_value.origin.x))
                               .Set(AXMakeOrderedKey(kYCoordDictKey, 1),
                                    static_cast<int>(rect_value.origin.y))
                               .Set(AXMakeOrderedKey(kWidthDictKey, 2),
                                    static_cast<int>(rect_value.size.width))
                               .Set(AXMakeOrderedKey(kHeightDictKey, 3),
                                    static_cast<int>(rect_value.size.height));
  return rect;
}

base::Value::Dict AXNSRangeToBaseValue(NSRange node_range) {
  base::Value::Dict range = base::Value::Dict()
                                .Set(AXMakeOrderedKey(kRangeLocDictKey, 0),
                                     static_cast<int>(node_range.location))
                                .Set(AXMakeOrderedKey(kRangeLenDictKey, 1),
                                     static_cast<int>(node_range.length));
  return range;
}

}  // namespace ui
