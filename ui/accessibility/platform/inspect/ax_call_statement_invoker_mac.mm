// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/inspect/ax_call_statement_invoker_mac.h"

#import <Accessibility/Accessibility.h>

#include "base/strings/sys_string_conversions.h"
#include "ui/accessibility/platform/ax_utils_mac.h"
#include "ui/accessibility/platform/inspect/ax_element_wrapper_mac.h"
#include "ui/accessibility/platform/inspect/ax_inspect_utils_mac.h"
#include "ui/accessibility/platform/inspect/ax_property_node.h"

namespace ui {

// Template specialization of AXOptional<id>::ToString().
template <>
std::string AXOptional<id>::ToString() const {
  if (IsNotNull())
    return base::SysNSStringToUTF8([NSString stringWithFormat:@"%@", value_]);
  return StateToString();
}

#define INT_FAIL(property_node, msg)                              \
  LOG(ERROR) << "Failed to parse " << property_node.name_or_value \
             << " to Int: " << msg;

#define INTARRAY_FAIL(property_node, msg)                         \
  LOG(ERROR) << "Failed to parse " << property_node.name_or_value \
             << " to IntArray: " << msg;

#define NSRANGE_FAIL(property_node, msg)                          \
  LOG(ERROR) << "Failed to parse " << property_node.name_or_value \
             << " to NSRange: " << msg;

#define UIELEMENT_FAIL(property_node, msg)                        \
  LOG(ERROR) << "Failed to parse " << property_node.name_or_value \
             << " to UIElement: " << msg;

#define TEXTMARKER_FAIL(property_node, msg)                                    \
  LOG(ERROR) << "Failed to parse " << property_node.name_or_value              \
             << " to AXTextMarker: " << msg                                    \
             << ". Expected format: {anchor, offset, affinity}, where anchor " \
                "is :line_num, offset is integer, affinity is either down, "   \
                "up or none";

AXCallStatementInvoker::AXCallStatementInvoker(
    const AXTreeIndexerMac* indexer,
    std::map<std::string, id>* storage)
    : node(nullptr), indexer_(indexer), storage_(storage) {}

AXCallStatementInvoker::AXCallStatementInvoker(const id node,
                                               const AXTreeIndexerMac* indexer)
    : node(node), indexer_(indexer), storage_(nullptr) {}

AXOptionalNSObject AXCallStatementInvoker::Invoke(
    const AXPropertyNode& property_node,
    bool no_object_parse) const {
  // TODO(alexs): failing the tests when filters are incorrect is a good idea,
  // however crashing ax_dump tools on wrong input might be not. Figure out
  // a working solution that works nicely in both cases. Use LOG(ERROR) for now
  // as a console warning.

  // Executes a scripting statement coded in a given property node.
  // The statement represents a chainable sequence of attribute calls, where
  // each subsequent call is invoked on an object returned by a previous call.
  // For example, p.AXChildren[0].AXRole will unroll into a sequence of
  // `p.AXChildren`, `(p.AXChildren)[0]` and `((p.AXChildren)[0]).AXRole`.

  // Get an initial target to invoke an attribute for. First, check the storage
  // if it has an associated target for the property node, then query the tree
  // indexer if the property node refers to a DOM id or line index of
  // an accessible object. If the property node doesn't provide a target then
  // use the default one (if any, the default node is provided in case of
  // a tree dumping only, the scripts never have default target).
  id target = nil;

  // Case 1: try to get a target from the storage. The target may refer to
  // a variable which is kept in the storage. For example,
  // `text_leaf:= p.AXChildren[0]` will define `text_leaf` variable and put it
  // into the storage, and then the variable value will be extracted from
  // the storage for other instruction referring the variable, for example,
  // `text_leaf.AXRole`.
  if (storage_) {
    auto storage_iterator = storage_->find(property_node.name_or_value);
    if (storage_iterator != storage_->end()) {
      target = storage_iterator->second;
      if (!target)
        return AXOptionalNSObject(target);
    }
  }
  // Case 2: try to get target from the tree indexer. The target may refer to
  // an accessible element by DOM id or by a line number (:LINE_NUM format) in
  // a result accessible tree. The tree indexer keeps the mappings between
  // accessible elements and their DOM ids and line numbers.
  if (!target)
    target = indexer_->NodeBy(property_node.name_or_value);

  // Case 3: no target either indicates an error or default target (if
  // applicable) or the property node is an object or a scalar value (for
  // example, `0` in `AXChildren[0]` or [3, 4] integer array).
  if (!target) {
    // If default target is given, i.e. |node| is not null, then the target is
    // deemed and we use the default target. This case is about ax tree dumping
    // where a scripting instruction with no target are used. For example,
    // `AXRole` property filter means it is applied to all nodes and `AXRole`
    // attribute should be called for all nodes in the tree.
    if (IsDumpingTree()) {
      if (property_node.IsTarget()) {
        LOG(ERROR) << "Failed to parse '" << property_node.name_or_value
                   << "' target in '" << property_node.ToFlatString() << "'";
        return AXOptionalNSObject::Error();
      }
    } else if (no_object_parse) {
      return AXOptionalNSObject::NotApplicable();
    } else {
      // Object or scalar case.
      target = PropertyNodeToNSObject(property_node);
      if (!target) {
        LOG(ERROR) << "Failed to parse '" << property_node.ToFlatString()
                   << "' to NSObject";
        return AXOptionalNSObject::Error();
      }
    }
  }

  // If target is deemed, then start from the given property node. Otherwise the
  // given property node is a target, and its next property node is a
  // method/property to invoke.
  auto* current_node = &property_node;
  if (target) {
    current_node = property_node.next.get();
  } else {
    target = node;
  }

  // Invoke the call chain.
  while (current_node) {
    auto target_optional = InvokeFor(target, *current_node);
    // Result of the current step is state. Don't go any further.
    if (!target_optional.HasValue())
      return target_optional;

    target = *target_optional;
    current_node = current_node->next.get();
  }

  // Variable case: store the variable value in the storage.
  if (!property_node.key.empty())
    (*storage_)[property_node.key] = target;

  // When dumping tree, return NULL values as NotApplicable in order to
  // easily filter them out of the dump.
  return IsDumpingTree() ? AXOptionalNSObject::NotNullOrNotApplicable(target)
                         : AXOptionalNSObject(target);
}

AXOptionalNSObject AXCallStatementInvoker::InvokeFor(
    const id target,
    const AXPropertyNode& property_node) const {
  if (target == nil) {
    return AXOptionalNSObject::Error(
        "Cannot call '" + property_node.ToFlatString() + "' on null value");
  }

  if (AXElementWrapper::IsValidElement(target)) {
    return InvokeForAXElement(AXElementWrapper{target}, property_node);
  }

  if (IsAXTextMarkerRange(target)) {
    return InvokeForAXTextMarkerRange(target, property_node);
  }

  if ([target isKindOfClass:[NSArray class]]) {
    return InvokeForArray(target, property_node);
  }

  if ([target isKindOfClass:[NSDictionary class]]) {
    return InvokeForDictionary(target, property_node);
  }

  if ([target isKindOfClass:[AXCustomContent class]]) {
    return InvokeForAXCustomContent(target, property_node);
  }

  LOG(ERROR) << "Unexpected target type for " << property_node.ToFlatString();
  return AXOptionalNSObject::Error();
}

AXOptionalNSObject AXCallStatementInvoker::InvokeForAXCustomContent(
    const id target,
    const AXPropertyNode& property_node) const {
  AXCustomContent* content = target;

  if (property_node.name_or_value == "label") {
    return AXOptionalNSObject(content.label);
  }
  if (property_node.name_or_value == "value") {
    return AXOptionalNSObject(content.value);
  }

  return AXOptionalNSObject::Error(
      "Unrecognized '" + property_node.name_or_value +
      "' attribute called on AXCustomContent object.");
}

AXOptionalNSObject AXCallStatementInvoker::InvokeForAXElement(
    const AXElementWrapper& ax_element,
    const AXPropertyNode& property_node) const {
  // Actions.
  if (property_node.name_or_value == "AXActionNames") {
    return AXOptionalNSObject::NotNullOrNotApplicable(ax_element.ActionNames());
  }
  if (property_node.name_or_value == "AXPerformAction") {
    AXOptionalNSObject param = ParamFrom(property_node);
    if (param.IsNotNull()) {
      ax_element.PerformAction(*param);
      return AXOptionalNSObject::Unsupported();
    }
    return AXOptionalNSObject::Error();
  }

  // Get or set attribute value if the attribute is supported.
  for (NSString* attribute : ax_element.AttributeNames()) {
    if (property_node.IsMatching(base::SysNSStringToUTF8(attribute))) {
      // Setter
      if (property_node.rvalue) {
        AXOptionalNSObject rvalue = Invoke(*property_node.rvalue);
        if (rvalue.IsNotNull()) {
          ax_element.SetAttributeValue(attribute, *rvalue);
          return {rvalue};
        }
        return rvalue;
      }
      // Getter. Make sure to expose null values in ax scripts.
      AXOptionalNSObject optional_value =
          ax_element.GetAttributeValue(attribute);
      return IsDumpingTree()
                 ? AXOptionalNSObject::NotNullOrNotApplicable(*optional_value)
                 : optional_value;
    }
  }

  // Parameterized attributes.
  for (NSString* attribute : ax_element.ParameterizedAttributeNames()) {
    if (property_node.IsMatching(base::SysNSStringToUTF8(attribute))) {
      AXOptionalNSObject param = ParamFrom(property_node);
      if (param.IsNotNull()) {
        return ax_element.GetParameterizedAttributeValue(attribute, *param);
      }
      return param;
    }
  }

  // Only invoke methods whose names start with "accessibility", or
  // "isAccessibility" that is specific to NSAccessibility or UIAccessibility
  // protocols if the object responds to a corresponding selector. Ignore all
  // other selectors the object may respond to because it exposes unwanted
  // NSAccessibility attributes listed in default filters as a side effect.
  //
  // Methods whose names start with "isAccessibility" returns a BOOL, so we
  // need to handle the returned value differently than methods whose return
  // types are id.
  if (base::StartsWith(property_node.name_or_value, "isAccessibility")) {
    std::optional<SEL> optional_arg_selector;
    std::string selector_string = property_node.name_or_value;
    // In some cases, we might want to pass a SEL as argument instead of an id.
    // When an argument is prefixed with "@SEL:", transform the string into a
    // valid SEL to pass to the main selector.
    if (property_node.arguments.size() == 1 &&
        base::StartsWith(property_node.arguments[0].name_or_value, "@SEL:")) {
      optional_arg_selector = NSSelectorFromString(base::SysUTF8ToNSString(
          property_node.arguments[0].name_or_value.substr(5)));
      selector_string += ":";
    }

    SEL selector =
        NSSelectorFromString(base::SysUTF8ToNSString(selector_string));
    if (!ax_element.RespondsToSelector(selector))
      return AXOptionalNSObject::Error();

    BOOL return_value =
        optional_arg_selector
            ? ax_element.Invoke<BOOL, SEL>(selector, *optional_arg_selector)
            : ax_element.Invoke<BOOL>(selector);
    return AXOptionalNSObject(@(return_value));
  }

  if (property_node.name_or_value == "setAccessibilityFocused")
    return InvokeSetAccessibilityFocused(ax_element, property_node);

  // accessibilityAttributeValue
  if (property_node.name_or_value == "accessibilityAttributeValue") {
    if (property_node.arguments.size() == 1) {
      return AXOptionalNSObject(ax_element.GetAttributeValue(
          base::SysUTF8ToNSString(property_node.arguments[0].name_or_value)));
    }
    // Parameterized accessibilityAttributeValue.
    if (property_node.arguments.size() == 2) {
      const std::string& attribute = property_node.arguments[0].name_or_value;
      AXOptionalNSObject param =
          ParamFrom(attribute, property_node.arguments[1]);
      if (!param.HasValue())
        return AXOptionalNSObject::Error();

      return AXOptionalNSObject(ax_element.GetParameterizedAttributeValue(
          base::SysUTF8ToNSString(attribute), *param));
    }
    return AXOptionalNSObject::Error();
  }

  if (base::StartsWith(property_node.name_or_value, "accessibility")) {
    if (property_node.arguments.size() == 1) {
      std::optional<id> optional_id =
          ax_element.PerformSelector(property_node.name_or_value,
                                     property_node.arguments[0].name_or_value);
      if (optional_id) {
        return AXOptionalNSObject(*optional_id);
      }
    }
    if (property_node.arguments.empty()) {
      auto optional_id =
          ax_element.PerformSelector(property_node.name_or_value);
      if (optional_id) {
        return AXOptionalNSObject(*optional_id);
      }
    }
  }

  // Unmatched attribute.
  // * We choose not to return an error when dumping the accessibility tree,
  // because during this process the same set of NSAccessibility attributes
  // listed in property filters are queried on all nodes and, naturally, not all
  // nodes support all attributes.
  // * We also explicitly choose not to return an error if the NSAccessibility
  // attribute is valid and is in the list of attributes that our tree formatter
  // supports, but is not exposed on a given node.
  if (IsDumpingTree() || IsValidAXAttribute(property_node.name_or_value)) {
    return AXOptionalNSObject::NotApplicable();
  }

  LOG(ERROR) << "Unrecognized '" << property_node.name_or_value
             << "' attribute called on AXElement in '"
             << property_node.ToFlatString() << "' statement";
  return AXOptionalNSObject::Error();
}

AXOptionalNSObject AXCallStatementInvoker::InvokeForAXTextMarkerRange(
    const id target,
    const AXPropertyNode& property_node) const {
  if (property_node.name_or_value == "anchor")
    return AXOptionalNSObject(AXTextMarkerRangeStart(target));

  if (property_node.name_or_value == "focus")
    return AXOptionalNSObject(AXTextMarkerRangeEnd(target));

  // Unmatched attribute. We choose not to return an error when dumping the
  // accessibility tree, because during this process the same set of
  // NSAccessibility attributes listed in property filters are queried on all
  // nodes and, naturally, not all nodes support all attributes.
  if (IsDumpingTree())
    return AXOptionalNSObject::Unsupported();

  LOG(ERROR) << "Unrecognized '" << property_node.name_or_value
             << "' attribute called on AXTextMarkerRange in '"
             << property_node.ToFlatString() << "' statement";
  return AXOptionalNSObject::Error();
}

AXOptionalNSObject AXCallStatementInvoker::InvokeForArray(
    const id target,
    const AXPropertyNode& property_node) const {
  if (property_node.name_or_value == "count") {
    if (property_node.arguments.size()) {
      LOG(ERROR) << "count attribute is called as a method";
      return AXOptionalNSObject::Error();
    }
    return AXOptionalNSObject([NSNumber numberWithInt:[target count]]);
  }

  if (property_node.name_or_value == "has") {
    for (NSString* attribute : target) {
      if (property_node.arguments[0].IsMatching(
              base::SysNSStringToUTF8(attribute))) {
        return AXOptionalNSObject(static_cast<id>(@"yes"));
      }
    }
    return AXOptionalNSObject(static_cast<id>(@"no"));
  }

  if (!property_node.IsArray() || property_node.arguments.size() != 1) {
    LOG(ERROR) << "Array operator[] is expected, got: "
               << property_node.ToString();
    return AXOptionalNSObject::Error();
  }

  std::optional<int> maybe_index = property_node.arguments[0].AsInt();
  if (!maybe_index || *maybe_index < 0) {
    LOG(ERROR) << "Wrong index for array operator[], got: "
               << property_node.arguments[0].ToString();
    return AXOptionalNSObject::Error();
  }

  if (static_cast<int>([target count]) <= *maybe_index) {
    LOG(ERROR) << "Out of range array operator[] index, got: "
               << property_node.arguments[0].ToString()
               << ", length: " << [target count];
    return AXOptionalNSObject::Error();
  }

  return AXOptionalNSObject(target[*maybe_index]);
}

AXOptionalNSObject AXCallStatementInvoker::InvokeForDictionary(
    const id target,
    const AXPropertyNode& property_node) const {
  if (property_node.arguments.size() > 0) {
    LOG(ERROR) << "dictionary key is expected, got: "
               << property_node.ToString();
    return AXOptionalNSObject::Error();
  }

  NSString* key = PropertyNodeToString(property_node);
  NSDictionary* dictionary = target;
  return AXOptionalNSObject::NotNullOrError(dictionary[key]);
}

AXOptionalNSObject AXCallStatementInvoker::InvokeSetAccessibilityFocused(
    const AXElementWrapper& ax_element,
    const AXPropertyNode& property_node) const {
  std::string selector_string = property_node.name_or_value + ":";
  if (property_node.arguments.size() != 1) {
    LOG(ERROR) << "Wrong arguments number for " << selector_string
               << ", got: " << property_node.arguments.size()
               << ", expected: 1";
    return AXOptionalNSObject::Error();
  }

  SEL selector = NSSelectorFromString(base::SysUTF8ToNSString(selector_string));
  if (!ax_element.RespondsToSelector(selector)) {
    LOG(ERROR) << "Target doesn't answer to " << selector_string << " selector";
    return AXOptionalNSObject::Error();
  }

  BOOL val = property_node.arguments[0].name_or_value == "FALSE" ? FALSE : TRUE;
  ax_element.Invoke<void, BOOL>(selector, val);
  return AXOptionalNSObject(nil);
}

AXOptionalNSObject AXCallStatementInvoker::ParamFrom(
    const AXPropertyNode& property_node) const {
  // NSAccessibility attributes always take a single parameter.
  if (property_node.arguments.size() != 1) {
    LOG(ERROR) << "Failed to parse '" << property_node.ToFlatString()
               << "': single parameter is expected";
    return AXOptionalNSObject::Error();
  }

  return ParamFrom(property_node.name_or_value, property_node.arguments[0]);
}

AXOptionalNSObject AXCallStatementInvoker::ParamFrom(
    const std::string& attribute,
    const AXPropertyNode& argument) const {
  // Nested attribute case: attempt to invoke an attribute for an argument node.
  AXOptionalNSObject subvalue = Invoke(argument, /* no_object_parse= */ true);
  if (!subvalue.IsNotApplicable()) {
    return subvalue;
  }

  // Otherwise parse argument node value.
  if (attribute == "AXLineForIndex" ||
      attribute == "AXTextMarkerForIndex") {  // Int
    return AXOptionalNSObject::NotNullOrError(PropertyNodeToInt(argument));
  }
  if (attribute == "AXPerformAction") {
    return AXOptionalNSObject::NotNullOrError(PropertyNodeToString(argument));
  }
  if (attribute == "AXCellForColumnAndRow") {  // IntArray
    return AXOptionalNSObject::NotNullOrError(PropertyNodeToIntArray(argument));
  }
  if (attribute ==
      "AXTextMarkerRangeForUnorderedTextMarkers") {  // TextMarkerArray
    return AXOptionalNSObject::NotNullOrError(
        PropertyNodeToTextMarkerArray(argument));
  }
  if (attribute == "AXAttributedStringForRange" ||
      attribute == "AXStringForRange") {  // NSRange
    return AXOptionalNSObject::NotNullOrError(PropertyNodeToRange(argument));
  }
  if (attribute == "AXIndexForChildUIElement" ||
      attribute == "AXTextMarkerRangeForUIElement") {  // UIElement
    return AXOptionalNSObject::NotNullOrError(
        PropertyNodeToUIElement(argument));
  }
  if (attribute == "AXIndexForTextMarker" ||
      attribute == "AXNextWordEndTextMarkerForTextMarker" ||
      attribute ==
          "AXPreviousWordStartTextMarkerForTextMarker") {  // TextMarker
    return AXOptionalNSObject::NotNullOrError(
        PropertyNodeToTextMarker(argument));
  }
  if (attribute == "AXAttributedStringForTextMarkerRange" ||
      attribute == "AXSelectedTextMarkerRangeAttribute" ||
      attribute == "AXStringForTextMarkerRange") {  // TextMarkerRange
    return AXOptionalNSObject::NotNullOrError(
        PropertyNodeToTextMarkerRange(argument));
  }

  return AXOptionalNSObject::NotApplicable();
}

id AXCallStatementInvoker::PropertyNodeToNSObject(
    const AXPropertyNode& property_node) const {
  // Integer array
  id value = PropertyNodeToIntArray(property_node, false);
  if (value)
    return value;

  // NSRange
  value = PropertyNodeToRange(property_node, false);
  if (value)
    return value;

  // TextMarker
  value = PropertyNodeToTextMarker(property_node, true);
  if (value)
    return value;

  // TextMarker array
  value = PropertyNodeToTextMarkerArray(property_node, false);
  if (value)
    return value;

  // TextMarkerRange
  return PropertyNodeToTextMarkerRange(property_node, false);
}

// NSNumber. Format: integer.
NSNumber* AXCallStatementInvoker::PropertyNodeToInt(
    const AXPropertyNode& intnode,
    bool log_failure) const {
  std::optional<int> param = intnode.AsInt();
  if (!param) {
    if (log_failure)
      INT_FAIL(intnode, "not a number")
    return nil;
  }
  return [NSNumber numberWithInt:*param];
}

NSString* AXCallStatementInvoker::PropertyNodeToString(
    const AXPropertyNode& strnode,
    bool log_failure) const {
  std::string str = strnode.AsString();
  return base::SysUTF8ToNSString(str);
}

// NSArray of two NSNumber. Format: [integer, integer].
NSArray* AXCallStatementInvoker::PropertyNodeToIntArray(
    const AXPropertyNode& arraynode,
    bool log_failure) const {
  if (!arraynode.IsArray()) {
    if (log_failure)
      INTARRAY_FAIL(arraynode, "not array")
    return nil;
  }

  NSMutableArray* array =
      [[NSMutableArray alloc] initWithCapacity:arraynode.arguments.size()];
  for (const auto& paramnode : arraynode.arguments) {
    std::optional<int> param = paramnode.AsInt();
    if (!param) {
      if (log_failure)
        INTARRAY_FAIL(arraynode, paramnode.name_or_value + " is not a number")
      return nil;
    }
    [array addObject:@(*param)];
  }
  return array;
}

// NSArray of AXTextMarker objects.
NSArray* AXCallStatementInvoker::PropertyNodeToTextMarkerArray(
    const AXPropertyNode& arraynode,
    bool log_failure) const {
  if (!arraynode.IsArray()) {
    if (log_failure)
      INTARRAY_FAIL(arraynode, "not array")
    return nil;
  }

  NSMutableArray* array =
      [[NSMutableArray alloc] initWithCapacity:arraynode.arguments.size()];
  for (const auto& paramnode : arraynode.arguments) {
    AXOptionalNSObject text_marker = Invoke(paramnode);
    if (!text_marker.IsNotNull()) {
      if (log_failure)
        INTARRAY_FAIL(arraynode,
                      paramnode.ToFlatString() + "is not a text marker")
      return nil;
    }
    [array addObject:(*text_marker)];
  }
  return array;
}

// NSRange. Format: {loc: integer, len: integer}.
NSValue* AXCallStatementInvoker::PropertyNodeToRange(
    const AXPropertyNode& dictnode,
    bool log_failure) const {
  if (!dictnode.IsDict()) {
    if (log_failure)
      NSRANGE_FAIL(dictnode, "dictionary is expected")
    return nil;
  }

  std::optional<int> loc = dictnode.FindIntKey("loc");
  if (!loc) {
    if (log_failure)
      NSRANGE_FAIL(dictnode, "no loc or loc is not a number")
    return nil;
  }

  std::optional<int> len = dictnode.FindIntKey("len");
  if (!len) {
    if (log_failure)
      NSRANGE_FAIL(dictnode, "no len or len is not a number")
    return nil;
  }

  return [NSValue valueWithRange:NSMakeRange(*loc, *len)];
}

// UIElement. Format: :line_num.
gfx::NativeViewAccessible AXCallStatementInvoker::PropertyNodeToUIElement(
    const AXPropertyNode& uielement_node,
    bool log_failure) const {
  gfx::NativeViewAccessible uielement =
      indexer_->NodeBy(uielement_node.name_or_value);
  if (!uielement) {
    if (log_failure)
      UIELEMENT_FAIL(uielement_node,
                     "no corresponding UIElement was found in the tree")
    return nil;
  }
  return uielement;
}

id AXCallStatementInvoker::DictionaryNodeToTextMarker(
    const AXPropertyNode& dictnode,
    bool log_failure) const {
  if (!dictnode.IsDict()) {
    if (log_failure)
      TEXTMARKER_FAIL(dictnode, "dictionary is expected")
    return nil;
  }
  if (dictnode.arguments.size() != 3) {
    if (log_failure)
      TEXTMARKER_FAIL(dictnode, "wrong number of dictionary elements")
    return nil;
  }

  AXPlatformNodeCocoa* anchor_cocoa = static_cast<AXPlatformNodeCocoa*>(
      indexer_->NodeBy(dictnode.arguments[0].name_or_value));
  if (!anchor_cocoa) {
    if (log_failure)
      TEXTMARKER_FAIL(dictnode, "1st argument: wrong anchor")
    return nil;
  }

  std::optional<int> offset = dictnode.arguments[1].AsInt();
  if (!offset) {
    if (log_failure)
      TEXTMARKER_FAIL(dictnode, "2nd argument: wrong offset")
    return nil;
  }

  ax::mojom::TextAffinity affinity;
  const std::string& affinity_str = dictnode.arguments[2].name_or_value;
  if (affinity_str == "none") {
    affinity = ax::mojom::TextAffinity::kNone;
  } else if (affinity_str == "down") {
    affinity = ax::mojom::TextAffinity::kDownstream;
  } else if (affinity_str == "up") {
    affinity = ax::mojom::TextAffinity::kUpstream;
  } else {
    if (log_failure)
      TEXTMARKER_FAIL(dictnode, "3rd argument: wrong affinity")
    return nil;
  }

  return AXTextMarkerFrom(anchor_cocoa, *offset, affinity);
}

id AXCallStatementInvoker::PropertyNodeToTextMarker(
    const AXPropertyNode& dictnode,
    bool log_failure) const {
  return DictionaryNodeToTextMarker(dictnode, log_failure);
}

id AXCallStatementInvoker::PropertyNodeToTextMarkerRange(
    const AXPropertyNode& rangenode,
    bool log_failure) const {
  if (!rangenode.IsDict()) {
    if (log_failure)
      TEXTMARKER_FAIL(rangenode, "dictionary is expected")
    return nil;
  }

  const AXPropertyNode* anchornode = rangenode.FindKey("anchor");
  if (!anchornode) {
    if (log_failure)
      TEXTMARKER_FAIL(rangenode, "no anchor")
    return nil;
  }

  id anchor_textmarker = DictionaryNodeToTextMarker(*anchornode);
  if (!anchor_textmarker) {
    if (log_failure)
      TEXTMARKER_FAIL(rangenode, "failed to parse anchor")
    return nil;
  }

  const AXPropertyNode* focusnode = rangenode.FindKey("focus");
  if (!focusnode) {
    if (log_failure)
      TEXTMARKER_FAIL(rangenode, "no focus")
    return nil;
  }

  id focus_textmarker = DictionaryNodeToTextMarker(*focusnode);
  if (!focus_textmarker) {
    if (log_failure)
      TEXTMARKER_FAIL(rangenode, "failed to parse focus")
    return nil;
  }

  return AXTextMarkerRangeFrom(anchor_textmarker, focus_textmarker);
}

}  // namespace ui
