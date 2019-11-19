// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/test/ns_ax_tree_validator.h"

#include <Cocoa/Cocoa.h>

#include "base/strings/sys_string_conversions.h"

namespace {

id<NSAccessibility> ToNSAccessibility(id obj) {
  return [obj conformsToProtocol:@protocol(NSAccessibility)] ? obj : nil;
}

void PrintNSAXTreeHelper(id<NSAccessibility> root, int depth) {
  std::string desc;
  for (int i = 0; i < depth; i++)
    desc += "  ";
  desc += base::SysNSStringToUTF8([NSString stringWithFormat:@"%@", root]);
  LOG(INFO) << desc;
  for (id child in root.accessibilityChildren)
    PrintNSAXTreeHelper(child, depth + 1);
}

}

namespace ui {

NSAXTreeProblemDetails::NSAXTreeProblemDetails(ProblemType type,
                                               id node_a,
                                               id node_b,
                                               id node_c)
    : type(type), node_a(node_a), node_b(node_b), node_c(node_c) {}

std::string NSAXTreeProblemDetails::ToString() {
  NSString* s;
  switch (type) {
    case NSAX_NOT_CHILD_OF_PARENT:
      s = [NSString
          stringWithFormat:@"Node %@ isn't a child of %@", node_a, node_b];
      break;
    case NSAX_CHILD_PARENT_NOT_THIS:
      s = [NSString stringWithFormat:@"Node %@'s child %@'s parent is %@",
                                     node_a, node_b, node_c];
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

base::Optional<NSAXTreeProblemDetails> ValidateNSAXTree(
    id<NSAccessibility> root,
    size_t* nodes_visited) {
  if (!ToNSAccessibility(root)) {
    return base::make_optional<NSAXTreeProblemDetails>(
        NSAXTreeProblemDetails::NSAX_NOT_NSACCESSIBILITY, root, nil, nil);
  }
  (*nodes_visited)++;

  if (root.accessibilityParent) {
    id<NSAccessibility> parent = ToNSAccessibility(root.accessibilityParent);
    if (!parent) {
      return base::make_optional<NSAXTreeProblemDetails>(
          NSAXTreeProblemDetails::NSAX_PARENT_NOT_NSACCESSIBILITY, root, parent,
          nil);
    }

    NSArray<id<NSAccessibility>>* parent_children =
        parent.accessibilityChildren;

    if ([parent_children indexOfObjectIdenticalTo:root] == NSNotFound) {
      return base::make_optional<NSAXTreeProblemDetails>(
          NSAXTreeProblemDetails::NSAX_NOT_CHILD_OF_PARENT, root, parent, nil);
    }
  }

  NSArray<id<NSAccessibility>>* children = root.accessibilityChildren;
  for (id<NSAccessibility> child in children) {
    base::Optional<NSAXTreeProblemDetails> details =
        ValidateNSAXTree(child, nodes_visited);
    if (details.has_value())
      return details;
  }

  return base::nullopt;
}

void PrintNSAXTree(id<NSAccessibility> root) {
  PrintNSAXTreeHelper(root, 0);
}

}  // namespace ui
