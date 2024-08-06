// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/page_transition_types.h"

#include <ostream>

#include "base/check_op.h"
#include "base/notreached.h"
#include "base/types/cxx23_to_underlying.h"

namespace ui {

bool PageTransitionCoreTypeIs(PageTransition lhs,
                              PageTransition rhs) {
  // Expect the rhs to have no qualifiers.
  DCHECK(IsValidPageTransitionType(base::to_underlying(rhs)));
  const auto rhs_core = PageTransitionStripQualifier(rhs);
  DCHECK(PageTransitionTypeIncludingQualifiersIs(rhs, rhs_core));
  return PageTransitionTypeIncludingQualifiersIs(
      PageTransitionStripQualifier(lhs), rhs_core);
}

bool PageTransitionTypeIncludingQualifiersIs(PageTransition lhs,
                                             PageTransition rhs) {
  return base::to_underlying(lhs) == base::to_underlying(rhs);
}

PageTransition PageTransitionStripQualifier(PageTransition type) {
  return static_cast<PageTransition>(type & ~PAGE_TRANSITION_QUALIFIER_MASK);
}

bool IsValidPageTransitionType(int32_t type) {
  return (type & ~PAGE_TRANSITION_QUALIFIER_MASK) <= PAGE_TRANSITION_LAST_CORE;
}

PageTransition PageTransitionFromInt(int32_t type) {
  CHECK(IsValidPageTransitionType(type))
      << "Invalid transition type: " << type
      << ". Untrusted data needs to be validated using "
         "IsValidPageTransitionType().";
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

PageTransition PageTransitionGetQualifier(PageTransition type) {
  return static_cast<PageTransition>(type & PAGE_TRANSITION_QUALIFIER_MASK);
}

bool PageTransitionIsWebTriggerable(PageTransition type) {
  const PageTransition t = PageTransitionStripQualifier(type);
  switch (t) {
    case PAGE_TRANSITION_LINK:
    case PAGE_TRANSITION_AUTO_SUBFRAME:
    case PAGE_TRANSITION_MANUAL_SUBFRAME:
    case PAGE_TRANSITION_FORM_SUBMIT:
      return true;
    default:
      return false;
  }
}

const char* PageTransitionGetCoreTransitionString(PageTransition type) {
  const PageTransition t = PageTransitionStripQualifier(type);
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
    default:
      NOTREACHED();
  }
}

}  // namespace ui
