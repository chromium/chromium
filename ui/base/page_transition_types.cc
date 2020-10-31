// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/page_transition_types.h"

#include <ostream>

#include "base/check_op.h"
#include "base/notreached.h"

namespace ui {

bool PageTransitionCoreTypeIs(PageTransition lhs,
                              PageTransition rhs) {
  // Expect the rhs to be a compile time constant without qualifiers.
  DCHECK_EQ(PageTransitionGetQualifier(rhs), 0);
  DCHECK(PageTransitionIsValidType(rhs));
  return static_cast<int32_t>(PageTransitionStripQualifier(lhs)) ==
      static_cast<int32_t>(PageTransitionStripQualifier(rhs));
}

bool PageTransitionTypeIncludingQualifiersIs(PageTransition lhs,
                                             PageTransition rhs) {
  return static_cast<int32_t>(lhs) == static_cast<int32_t>(rhs);
}

PageTransition PageTransitionStripQualifier(PageTransition type) {
  return static_cast<PageTransition>(type & ~PAGE_TRANSITION_QUALIFIER_MASK);
}

bool PageTransitionIsValidType(int32_t type) {
  PageTransition t = PageTransitionStripQualifier(
      static_cast<PageTransition>(type));
  return (t <= PAGE_TRANSITION_LAST_CORE);
}

PageTransition PageTransitionFromInt(int32_t type) {
  if (!PageTransitionIsValidType(type)) {
    NOTREACHED() << "Invalid transition type " << type;

    // Return a safe default so we don't have corrupt data in release mode.
    return PAGE_TRANSITION_LINK;
  }
  return static_cast<PageTransition>(type);
}

bool PageTransitionIsMainFrame(PageTransition type) {
  return !PageTransitionCoreTypeIs(type, PAGE_TRANSITION_AUTO_SUBFRAME) &&
         !PageTransitionCoreTypeIs(type, PAGE_TRANSITION_MANUAL_SUBFRAME);
}

bool PageTransitionIsRedirect(PageTransition type) {
  return (type & PAGE_TRANSITION_IS_REDIRECT_MASK) != 0;
}

bool PageTransitionIsNewNavigation(PageTransition type) {
  return (type & PAGE_TRANSITION_FORWARD_BACK) == 0 &&
      !PageTransitionCoreTypeIs(type, PAGE_TRANSITION_RELOAD);
}

int32_t PageTransitionGetQualifier(PageTransition type) {
  return type & PAGE_TRANSITION_QUALIFIER_MASK;
}

bool PageTransitionIsWebTriggerable(PageTransition type) {
  int32_t t = PageTransitionStripQualifier(type);
  switch (t) {
    case PAGE_TRANSITION_LINK:
    case PAGE_TRANSITION_AUTO_SUBFRAME:
    case PAGE_TRANSITION_MANUAL_SUBFRAME:
    case PAGE_TRANSITION_FORM_SUBMIT:
      return true;
  }
  return false;
}

const char* PageTransitionGetCoreTransitionString(PageTransition type) {
  int32_t t = PageTransitionStripQualifier(type);
  switch (t) {
    case PAGE_TRANSITION_LINK: return "link";
    case PAGE_TRANSITION_TYPED: return "typed";
    case PAGE_TRANSITION_AUTO_BOOKMARK: return "auto_bookmark";
    case PAGE_TRANSITION_AUTO_SUBFRAME: return "auto_subframe";
    case PAGE_TRANSITION_MANUAL_SUBFRAME: return "manual_subframe";
    case PAGE_TRANSITION_GENERATED: return "generated";
    case PAGE_TRANSITION_AUTO_TOPLEVEL: return "auto_toplevel";
    case PAGE_TRANSITION_FORM_SUBMIT: return "form_submit";
    case PAGE_TRANSITION_RELOAD: return "reload";
    case PAGE_TRANSITION_KEYWORD: return "keyword";
    case PAGE_TRANSITION_KEYWORD_GENERATED: return "keyword_generated";
    case PAGE_TRANSITION_FROM_API_3:
      return "api3";
  }
  return NULL;
}

}  // namespace ui
