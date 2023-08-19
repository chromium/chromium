// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_CALL_STATEMENT_INVOKER_MAC_H_
#define UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_CALL_STATEMENT_INVOKER_MAC_H_

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "ui/accessibility/platform/inspect/ax_tree_indexer_mac.h"

namespace ui {

class AXElementWrapper;
class AXPropertyNode;

// Invokes a script instruction describing a call unit which represents
// a sequence of calls.
class COMPONENT_EXPORT(AX_PLATFORM) AXCallStatementInvoker final {
 public:
  // Generic version, all calls are executed in the context of property nodes.
  // Note: both |indexer| and |storage| must outlive this object.
  AXCallStatementInvoker(const AXTreeIndexerMac* indexer,
                         std::map<std::string, id>* storage);

  // Single target version, all calls are executed in the context of the given
  // target node.
  // Note: |indexer| must outlive this object.
  AXCallStatementInvoker(const id node, const AXTreeIndexerMac* indexer);

  // Invokes an attribute matching a property filter.
  AXOptionalNSObject Invoke(const AXPropertyNode& property_node,
                            bool no_object_parse = false) const;

 private:
  // Returns true if the invoker is instantiated to invoke an ax_script
  // instruction, in contrast to processing ax_dump_tree filters.
  bool IsDumpingTree() const { return !!node; }

  // Invokes a property node for a given target.
  AXOptionalNSObject InvokeFor(const id target,
                               const AXPropertyNode& property_node) const;

  // Invoke a property node for a given AXCustomContent.
  AXOptionalNSObject InvokeForAXCustomContent(
      const id target,
      const AXPropertyNode& property_node) const;

  // Invokes a property node for a given AXElement.
  AXOptionalNSObject InvokeForAXElement(
      const AXElementWrapper& ax_element,
      const AXPropertyNode& property_node) const;

  // Invokes a property node for a given AXTextMarkerRange.
  AXOptionalNSObject InvokeForAXTextMarkerRange(
      const id target,
      const AXPropertyNode& property_node) const;

  // Invokes a property node for a given array.
  AXOptionalNSObject InvokeForArray(const id target,
                                    const AXPropertyNode& property_node) const;

  // Invokes a property node for a given dictionary.
  AXOptionalNSObject InvokeForDictionary(
      const id target,
      const AXPropertyNode& property_node) const;

  // Invokes setAccessibilityFocused method.
  AXOptionalNSObject InvokeSetAccessibilityFocused(
      const AXElementWrapper& ax_element,
      const AXPropertyNode& property_node) const;

  // Returns a parameterized attribute parameter by a property node representing
  // an attribute call.
  AXOptionalNSObject ParamFrom(const AXPropertyNode&) const;

  // Returns a parameterized attribute parameter by an attribute and a property
  // node representing an argument.
  AXOptionalNSObject ParamFrom(const std::string& attribute,
                               const AXPropertyNode& argument) const;

  // Converts a given property node to NSObject. If not convertible, returns
  // nil.
  id PropertyNodeToNSObject(const AXPropertyNode& property_node) const;

  NSNumber* PropertyNodeToInt(const AXPropertyNode&,
                              bool log_failure = true) const;
  NSString* PropertyNodeToString(const AXPropertyNode&,
                                 bool log_failure = true) const;
  NSArray* PropertyNodeToIntArray(const AXPropertyNode&,
                                  bool log_failure = true) const;
  NSArray* PropertyNodeToTextMarkerArray(const AXPropertyNode&,
                                         bool log_failure = true) const;
  NSValue* PropertyNodeToRange(const AXPropertyNode&,
                               bool log_failure = true) const;
  gfx::NativeViewAccessible PropertyNodeToUIElement(
      const AXPropertyNode&,
      bool log_failure = true) const;

  id DictionaryNodeToTextMarker(const AXPropertyNode&,
                                bool log_failure = true) const;
  id PropertyNodeToTextMarker(const AXPropertyNode&,
                              bool log_failure = true) const;
  id PropertyNodeToTextMarkerRange(const AXPropertyNode&,
                                   bool log_failure = true) const;

  gfx::NativeViewAccessible LineIndexToNode(
      const std::u16string line_index) const;

  id __strong node;

  // Map between AXUIElement objects and their DOMIds/accessible tree
  // line numbers. Owned by the caller and outlives this object.
  const raw_ptr<const AXTreeIndexerMac> indexer_;

  // Variables storage. Owned by the caller and outlives this object.
  const raw_ptr<std::map<std::string, id>> storage_;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_CALL_STATEMENT_INVOKER_MAC_H_
