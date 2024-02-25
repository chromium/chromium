// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/test/view_tree_validator.h"

#include <Cocoa/Cocoa.h>

#include "base/mac/mac_util.h"
#include "base/strings/sys_string_conversions.h"

namespace {

NSArray* CollectSubviews(NSView* root) {
  NSMutableArray* subviews = [NSMutableArray arrayWithObject:root];
  for (NSView* child in root.subviews) {
    [subviews addObjectsFromArray:CollectSubviews(child)];
  }
  return subviews;
}

bool ViewsOverlap(NSView* a, NSView* b) {
  NSRect a_frame = [a convertRect:a.bounds toView:nil];
  NSRect b_frame = [b convertRect:b.bounds toView:nil];
  return NSIntersectsRect(a_frame, b_frame);
}

bool IsLocalizable(NSView* view) {
  return [view isKindOfClass:[NSControl class]] ||
         [view isKindOfClass:[NSText class]];
}

// Returns whether to expect children of |view| to perhaps not fit within its
// bounds.
bool IgnoreChildBoundsChecks(NSView* view) {
  // On macOS 10.14+, NSButton has a subview of a private helper class whose
  // bounds extend a bit outside the NSButton itself. We don't care about this
  // helper class's bounds being outside the button.
  return [view isKindOfClass:[NSButton class]];
}

}  // namespace

namespace ui {

std::optional<ViewTreeProblemDetails> ValidateViewTree(NSView* root) {
  NSArray* allViews = CollectSubviews(root);

  for (NSView* view in allViews) {
    // 1: Check that every subview's frame lies entirely inside this view's
    // bounds.
    for (NSView* child in view.subviews) {
      if (!NSContainsRect(view.bounds, child.frame) &&
          !IgnoreChildBoundsChecks(view)) {
        return std::optional<ViewTreeProblemDetails>(
            {ViewTreeProblemDetails::ProblemType::kViewOutsideParent, child,
             view});
      }
    }

    // If |view| isn't localizable, skip the rest of the checks.
    if (!IsLocalizable(view))
      continue;

    // 2: Check that every other subview either:
    //   a: doesn't overlap this view
    //   b: is a descendant of this view
    //   c: has this view as a descendant
    // note that a view is its own descendant.
    for (NSView* other in allViews) {
      if (!ViewsOverlap(view, other))
        continue;
      if ([view isDescendantOf:other] || [other isDescendantOf:view])
        continue;
      return std::optional<ViewTreeProblemDetails>(
          {ViewTreeProblemDetails::ProblemType::kViewsOverlap, view, other});
    }
  }

  return std::nullopt;
}

std::string ViewTreeProblemDetails::ToString() {
  NSString* s;
  switch (type) {
    case ProblemType::kViewOutsideParent:
      s = [NSString stringWithFormat:@"View %@ [%@] outside parent %@ [%@]",
                                     view_a, NSStringFromRect(view_a.frame),
                                     view_b, NSStringFromRect(view_b.frame)];
      break;
    case ProblemType::kViewsOverlap:
      s = [NSString stringWithFormat:@"Views %@ [%@] and %@ [%@] overlap",
                                     view_a, NSStringFromRect(view_a.frame),
                                     view_b, NSStringFromRect(view_b.frame)];
      break;
  }

  return base::SysNSStringToUTF8(s);
}

}  // namespace ui
