// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/test/ns_ax_tree_validator.h"

#include <Cocoa/Cocoa.h>

#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"

namespace {

id<NSAccessibility> ToNSAccessibility(id obj) {
  return [obj conformsToProtocol:@protocol(NSAccessibility)] ? obj : nil;
}

void PrintNSAXTreeHelper(id<NSAccessibility> node, int depth) {
  std::string desc;
  for (int i = 0; i < depth; i++) {
    desc += "  ";
  }
  desc += base::SysNSStringToUTF8([NSString stringWithFormat:@"%@", node]);
  LOG(INFO) << desc;
  for (id child in node.accessibilityChildren) {
    PrintNSAXTreeHelper(child, depth + 1);
  }
}
}  // namespace

namespace ui {

NSAXTreeProblemDetails::NSAXTreeProblemDetails(ProblemType type,
                                               id node_a,
                                               id node_b)
    : type(type), node_a(node_a), node_b(node_b) {}

std::string NSAXTreeProblemDetails::ToString() {
  NSString* s;
  switch (type) {
    case NSAX_NOT_CHILD_OF_PARENT:
      s = [NSString
          stringWithFormat:@"Node %@ isn't a child of %@", node_a, node_b];
      break;
    case NSAX_NOT_NSACCESSIBILITY:
      s = [NSString stringWithFormat:@"Node %@ does not conform to"
                                      " to NSAccessibility",
                                     node_a];
      break;
    case NSAX_PARENT_NOT_NSACCESSIBILITY:
      s = [NSString stringWithFormat:@"Node %@'s parent %@ does not conform"
                                      " to NSAccessibility",
                                     node_a, node_b];
      break;
  }
  return base::SysNSStringToUTF8(s);
}

std::optional<NSAXTreeProblemDetails> ValidateNSAXTree(id<NSAccessibility> node,
                                                       size_t* nodes_visited) {
  if (!ToNSAccessibility(node)) {
    return std::make_optional<NSAXTreeProblemDetails>(
        NSAXTreeProblemDetails::NSAX_NOT_NSACCESSIBILITY, node, nil);
  }
  (*nodes_visited)++;

  // NSThemeWidgetZoomMenuRemoteView, a class new in macOS 14, violates
  // invariants; its actual accessibility parent chain is [NSWindow,
  // _NSThemeZoomWidgetCell] but when asked for its parent it returns the
  // NSWindow, which doesn't have it as a child. This is an invariant that
  // should hold (FB13557859). TODO(crbug.com/40935248): When FB13557859
  // is fixed, remove this workaround.
  bool skip_due_to_fb13557859 =
      node.class == NSClassFromString(@"NSThemeWidgetZoomMenuRemoteView");

  if (node.accessibilityParent && !skip_due_to_fb13557859) {
    id<NSAccessibility> parent = ToNSAccessibility(node.accessibilityParent);
    if (!parent) {
      return std::make_optional<NSAXTreeProblemDetails>(
          NSAXTreeProblemDetails::NSAX_PARENT_NOT_NSACCESSIBILITY, node,
          parent);
    }

    NSArray<id<NSAccessibility>>* parent_children =
        parent.accessibilityChildren;

    if (![parent_children containsObject:node]) {
      return std::make_optional<NSAXTreeProblemDetails>(
          NSAXTreeProblemDetails::NSAX_NOT_CHILD_OF_PARENT, node, parent);
    }
  }

  NSArray<id<NSAccessibility>>* children = node.accessibilityChildren;
  for (id<NSAccessibility> child in children) {
    std::optional<NSAXTreeProblemDetails> details =
        ValidateNSAXTree(child, nodes_visited);
    if (details.has_value()) {
      return details;
    }
  }

  return std::nullopt;
}

void PrintNSAXTree(id<NSAccessibility> root) {
  PrintNSAXTreeHelper(root, 0);
}

}  // namespace ui
